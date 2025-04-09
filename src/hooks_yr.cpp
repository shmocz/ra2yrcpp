#include "hooks_yr.hpp"

#include "ra2yrproto/commands_yr.pb.h"
#include "ra2yrproto/ra2yr.pb.h"

#include "auto_thread.hpp"
#include "config.hpp"
#include "hook.hpp"
#include "instrumentation_service.hpp"
#include "is_context.hpp"
#include "logging.hpp"
#include "protocol/helpers.hpp"
#include "ra2/abi.hpp"
#include "ra2/state_context.hpp"
#include "ra2/state_parser.hpp"
#include "ra2/yrpp_export.hpp"
#include "types.h"

#include <fmt/core.h>
#include <google/protobuf/repeated_ptr_field.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _MSC_VER
#pragma section(".syhks00", read, write)
#endif

using hook::HookEntry;

using namespace ra2yrcpp::hooks_yr;
using namespace std::chrono_literals;

static auto default_configuration() {
  ra2yrproto::commands::Configuration C;
  C.set_debug_log(true);
  C.set_parse_map_data_interval(1U);
  C.set_single_step(false);
  return C;
}

static auto load_configuration() {
  auto C = default_configuration();
  auto ts = std::to_string(static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count()));
  char* p = nullptr;
  std::string record_path, traffic_path;
  if ((p = std::getenv("RA2YRCPP_RECORD_PATH")) != nullptr) {
    record_path = p;
    if (record_path.empty()) {
      record_path = fmt::format("record.{}.pb.gz", ts);
    }
  }
  if ((p = std::getenv("RA2YRCPP_RECORD_TRAFFIC")) != nullptr) {
    traffic_path = p;
    if (traffic_path.empty()) {
      traffic_path = fmt::format("traffic.{}.pb.gz", ts);
    }
  }
  C.set_record_filename(record_path);
  C.set_traffic_filename(traffic_path);
  return C;
}

GameDataYR::GameDataYR() : cfg(load_configuration()) {
  ctx = std::make_unique<ra2::StateContext>(&abi, &sv);
}

ra2::abi::ABIGameMD* GameDataInterface::abi() { return &data()->abi; }

ra2yrproto::ra2yr::GameState* GameDataInterface::game_state() {
  return data()->sv.mutable_game_state();
}

GameDataInterface::tc_t* GameDataInterface::type_classes() {
  return data()->sv.mutable_initial_game_state()->mutable_object_types();
}

ra2::StateContext* GameDataInterface::get_state_context() {
  return data()->ctx.get();
}

ra2yrcpp::hooks_yr::GameDataYR* GameDataInterface::data() {
  if (data_ == nullptr) {
    data_ = MainData::get().data();
  }
  return data_;
}

void MainData::update_configuration(
    const ra2yrproto::commands::Configuration& C) {
  auto* cfg = &data()->cfg;
  if (C.parse_map_data_interval() > 0U) {
    cfg->set_parse_map_data_interval(C.parse_map_data_interval());
  }
  cfg->set_single_step(C.single_step());
}

ra2yrproto::ra2yr::PrerequisiteGroups*
GameDataInterface::prerequisite_groups() {
  return data()->sv.mutable_initial_game_state()->mutable_prerequisite_groups();
}

ra2yrproto::commands::Configuration* GameDataInterface::configuration() {
  return &data()->cfg;
}

template <typename T>
static T* get_service_data() {
  return reinterpret_cast<T*>(MainData::get().service_datas().at(T::id).get());
}

struct StateSave : public GameDataInterface {
  ra2yrcpp::protocol::MessageOstream out;
  utility::worker_util<std::shared_ptr<ra2yrproto::ra2yr::GameState>> work;
  ra2yrproto::ra2yr::GameState* initial_state;
  std::vector<ra2::Cell> cells;

  static constexpr auto id = ServiceDataId::STATE_SAVE;

  explicit StateSave(std::shared_ptr<std::ostream> record_stream)
      : out(record_stream, true),
        work([this](const auto& w) { this->serialize_state(*w.get()); }, 10U),
        initial_state(nullptr) {}

  void serialize_state(const ra2yrproto::ra2yr::GameState& G) {
    if (out.os != nullptr) {
      if (!out.write(G)) {
        throw std::runtime_error("write_message");
      }
    }
  }

  void update_MapData(
      ra2yrproto::ra2yr::MapData* M,
      const RepeatedPtrField<ra2yrproto::ra2yr::Cell>& difference) {
    for (const auto& c : difference) {
      M->mutable_cells()->at(c.index()).CopyFrom(c);
    }
  }

  std::shared_ptr<ra2yrproto::ra2yr::GameState> state_to_protobuf(
      const bool do_type_classes = false) {
    auto* sval = &data()->sv;
    auto* gbuf = sval->mutable_game_state();

    // put load stages
    gbuf->mutable_load_progresses()->CopyFrom(
        sval->load_state().load_progresses());

    gbuf->clear_object_types();
    gbuf->clear_prerequisite_groups();

    // Parse type classes only once
    if (do_type_classes) {
      ra2::parse_AbstractTypeClasses(type_classes(), abi());
      ra2::parse_prerequisiteGroups(prerequisite_groups());
      gbuf->mutable_object_types()->CopyFrom(*type_classes());
      gbuf->mutable_prerequisite_groups()->CopyFrom(*prerequisite_groups());
    }

    gbuf->set_crc(EventClass::CurrentFrameCRC);
    gbuf->set_current_frame(Unsorted::CurrentFrame);
    gbuf->set_tech_level(Game::TechLevel);
    ra2::parse_HouseClasses(gbuf);
    ra2::parse_Objects(gbuf, abi());
    ra2::parse_Factories(gbuf->mutable_factories());

    gbuf->set_stage(ra2yrproto::ra2yr::LoadStage::STAGE_INGAME);

    // Initialize MapData
    if (gbuf->current_frame() > 0U && sval->map_data().cells_size() == 0U) {
      ra2::parse_MapData(sval->mutable_map_data(), MapClass::Instance.get(),
                         abi());
    }

    if (cells.empty() && gbuf->current_frame() > 0U) {
      auto valid_cells = ra2::get_valid_cells(MapClass::Instance.get());
      cells = std::vector<ra2::Cell>(valid_cells.size());
    }

    // Parse cells
    if (!cells.empty() &&
        (gbuf->current_frame() % configuration()->parse_map_data_interval() ==
         0U)) {
      gbuf->clear_cells_difference();
      ra2::parse_map(&cells, MapClass::Instance.get(),
                     gbuf->mutable_cells_difference());
      update_MapData(sval->mutable_map_data(), gbuf->cells_difference());
    }

    if (initial_state == nullptr) {
      initial_state = data()->sv.mutable_initial_game_state();
      initial_state->CopyFrom(*gbuf);
    }

    ra2::parse_EventLists(gbuf, sval->mutable_event_buffer(),
                          cfg::EVENT_BUFFER_SIZE);

    return std::make_shared<ra2yrproto::ra2yr::GameState>(*gbuf);
  }

  static std::unique_ptr<StateSave> create(std::string record_path) {
    std::shared_ptr<std::ofstream> record_out = nullptr;
    if (!record_path.empty()) {
      iprintf("record state to {}", record_path);
      record_out = std::make_shared<std::ofstream>(
          record_path, std::ios_base::out | std::ios_base::binary);
    }
    return std::make_unique<StateSave>(record_out);
  }

  void execute() {
    auto st = state_to_protobuf(type_classes()->empty());
    work.push(st);
  }

  static StateSave* get() { return get_service_data<StateSave>(); }
};

struct SaveTrafficData : ServiceData {
  static constexpr ServiceDataId id = ServiceDataId::RECORD_TRAFFIC;
  using writer_t = std::shared_ptr<ra2yrcpp::protocol::MessageOstream>;
  writer_t out;

  struct packet_buffer {
    void* data;
    i32 size;  // set to -1 on error
    // these indicate just the packet direction: if receiving, source=1, if
    // sending, destination=1
    u32 source;
    u32 destination;
  };

  explicit SaveTrafficData(writer_t out) : out(out) {}

  packet_buffer recv_buffer(const X86Regs* cpu_state) {
    return {reinterpret_cast<void*>(cpu_state->ebp + 0x3f074),
            static_cast<i32>(cpu_state->esi), 1U, 0U};
  }

  packet_buffer send_buffer(const X86Regs* cpu_state) {
    return {reinterpret_cast<void*>(cpu_state->ecx),
            static_cast<i32>(cpu_state->eax), 0U, 1U};
  }

  void write_packet(u32 source, u32 dest, const void* buf, std::size_t len) {
    // dprintf("source={} dest={}, buf={}, len={}", source, dest, buf, len);
    ra2yrproto::ra2yr::TunnelPacket P;
    P.set_source(source);
    P.set_destination(dest);
    P.mutable_data()->assign(static_cast<const char*>(buf), len);
    if (!out->write(P)) {
      throw std::runtime_error("write_packet failed");
    }
  }

  void write_packet(packet_buffer b) {
    if (out != nullptr && b.size > 0) {
      write_packet(b.source, b.destination, b.data, b.size);
    }
  }

  static std::unique_ptr<SaveTrafficData> create(std::string traffic_out) {
    writer_t out = nullptr;
    if (!traffic_out.empty()) {
      out = std::make_shared<ra2yrcpp::protocol::MessageOstream>(
          std::make_shared<std::ofstream>(
              traffic_out, std::ios_base::out | std::ios_base::binary),
          true);
      iprintf("record traffic to {}", traffic_out);
    }

    return std::make_unique<SaveTrafficData>(out);
  }

  static SaveTrafficData* get() { return get_service_data<SaveTrafficData>(); }
};

void MainData::initialize_service_datas() {
  auto& C = data_->cfg;
  service_datas_.try_emplace(ServiceDataId::STATE_SAVE,
                             StateSave::create(C.record_filename()));
  service_datas_.try_emplace(ServiceDataId::GAME_COMMAND,
                             GameCommandData::create());
  service_datas_.try_emplace(ServiceDataId::RECORD_TRAFFIC,
                             SaveTrafficData::create(C.traffic_filename()));
}

void MainData::deinitialize_service_datas() { service_datas_.clear(); }

GameCommandData::GameCommandData() = default;

void GameCommandData::put_work(work_t fn) { work.push(fn); }

void GameCommandData::consume_work() {
  auto items = work.pop(0, 0.0s);
  for (const auto& it : items) {
    it();
  }
}

GameCommandData* GameCommandData::get() {
  return get_service_data<GameCommandData>();
}

std::unique_ptr<GameCommandData> GameCommandData::create() {
  return std::make_unique<GameCommandData>();
}

MainData* MainData::instance_ = nullptr;
std::recursive_mutex MainData::lock_;

MainData::MainData() : data_(std::make_unique<GameDataYR>()) {}

MainData& MainData::get() {
  if (instance_ == nullptr) {
    instance_ = new MainData();
  }
  return *instance_;
}

GameDataYR* MainData::data() { return data_.get(); }

void MainData::lock() { lock_.lock(); }

void MainData::unlock() { lock_.unlock(); }

std::map<ServiceDataId, std::unique_ptr<ServiceData>>&
MainData::service_datas() {
  return service_datas_;
}

util::acquire_t<MainData, std::recursive_mutex> MainData::acquire() {
  return util::acquire(&get(), &lock_);
}

DEFINE_HOOK(0x7b3d6f, TunnelSendTo, 0x6) {
  auto [mut, M] = MainData::acquire();

  try {
    auto* C = SaveTrafficData::get();
    C->write_packet(C->send_buffer(reinterpret_cast<X86Regs*>(R)));
  } catch (const std::out_of_range& e) {
    eprintf("ServiceData doesn't exist");
  }
  return 0U;
}

DEFINE_HOOK(0x7b3f15, TunnelRecvFrom, 0x6) {
  auto [mut, M] = MainData::acquire();

  try {
    auto* C = SaveTrafficData::get();
    C->write_packet(C->recv_buffer(reinterpret_cast<X86Regs*>(R)));
  } catch (const std::out_of_range& e) {
    eprintf("ServiceData doesn't exist");
  }
  return 0U;
}

DEFINE_HOOK(0x643c62, UpdateLoadProgress, 0x6) {
  (void)R;
  auto [mut, M] = MainData::acquire();

  auto* B = ProgressScreenClass::Instance().PlayerProgresses;

  auto* data = M->data();
  auto* sv = &data->sv;
  auto* local_state = sv->mutable_load_state();
  if (local_state->load_progresses().empty()) {
    for (auto i = 0U; i < (sizeof(*B) / sizeof(B)); i++) {
      local_state->add_load_progresses(0.0);
    }
  }
  for (int i = 0; i < local_state->load_progresses().size(); i++) {
    local_state->set_load_progresses(i, B[i]);
  }
  sv->mutable_game_state()->set_stage(
      ra2yrproto::ra2yr::LoadStage::STAGE_LOADING);
  return 0U;
}

DEFINE_HOOK(0x72dfb0, ExitGameLoop, 0x6) {
  (void)R;
  auto [mut, M] = MainData::acquire();

  GameCommandData::get()->game_state()->set_stage(
      ra2yrproto::ra2yr::STAGE_EXIT_GAME);
  // FIXME: Do this later, as tunnel hooks are still reached.
  M->deinitialize_service_datas();

  // Flush output in case the process is not terminated gracefully.
  std::cerr << std::flush;
  std::cout << std::flush;
  return 0U;
}

DEFINE_HOOK(0x55de4f, GameLoopBegin, 0x7) {
  (void)R;
  auto [mut, M] = MainData::acquire();

  // Save state
  StateSave::get()->execute();

  // If in single-step mode, release storage lock and wait for game to be
  // unlocked.
  auto* D = M->data();
  if (D->cfg.single_step()) {
    M->unlock();
    D->game_paused.store(true);
    D->game_paused.wait(false);
    M->lock();
  }

  GameCommandData::get()->consume_work();

  return 0U;
};

DEFINE_HOOK(0x7cd84d, ExeRun, 0x9) {
  (void)R;
  auto [mut, M] = MainData::acquire();
  M->initialize_service_datas();
  is_context::RA2YRCPP::get()->start_service();
  return 0U;
}
