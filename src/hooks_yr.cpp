#include "hooks_yr.hpp"

#include "ra2yrproto/commands_yr.pb.h"
#include "ra2yrproto/ra2yr.pb.h"

#include "auto_thread.hpp"
#include "config.hpp"
#include "hook.hpp"
#include "instrumentation_service.hpp"
#include "logging.hpp"
#include "protocol/helpers.hpp"
#include "ra2/abi.hpp"
#include "ra2/state_context.hpp"
#include "ra2/state_parser.hpp"
#include "ra2/yrpp_export.hpp"
#include "utility/serialize.hpp"

#include <fmt/core.h>
#include <google/protobuf/repeated_ptr_field.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace ra2yrcpp::hooks_yr;
using namespace std::chrono_literals;

static auto default_configuration() {
  ra2yrproto::commands::Configuration C;
  C.set_debug_log(true);
  C.set_parse_map_data_interval(1U);
  return C;
}

GameDataYR::GameDataYR() : cfg(default_configuration()) {
  ctx = std::make_unique<ra2::StateContext>(&abi, &sv);
}

cb_map_t* ra2yrcpp::hooks_yr::get_callbacks(
    ra2yrcpp::InstrumentationService* I) {
  return &get_data(I)->callbacks;
}

CBYR::CBYR() {}

ra2::abi::ABIGameMD* CBYR::abi() { return &data()->abi; }

void CBYR::do_call() {
  I->lock_storage();
  auto [mut_cc, cc] = abi()->acquire_code_generators();
  try {
    exec();
  } catch (const std::exception& e) {
    eprintf("{}: {}", name(), e.what());
  }
  I->unlock_storage();
}

ra2yrproto::ra2yr::GameState* CBYR::game_state() {
  return data()->sv.mutable_game_state();
}

CBYR::tc_t* CBYR::type_classes() {
  return data()->sv.mutable_initial_game_state()->mutable_object_types();
}

ra2::StateContext* CBYR::get_state_context() { return data()->ctx.get(); }

ra2yrcpp::hooks_yr::GameDataYR* CBYR::data() {
  return data_ != nullptr ? data_ : get_data(I);
}

auto* CBYR::prerequisite_groups() {
  return data()->sv.mutable_initial_game_state()->mutable_prerequisite_groups();
}

ra2yrproto::commands::Configuration* CBYR::configuration() {
  return &data()->cfg;
}

// TODO(shmocz): do the callback initialization later
struct CBExitGameLoop final
    : public MyCB<CBExitGameLoop, ra2yrcpp::ISCallback> {
  static constexpr char key_target[] = "on_gameloop_exit";
  static constexpr char key_name[] = "gameloop_exit";

  CBExitGameLoop() = default;
  CBExitGameLoop(const CBExitGameLoop& o) = delete;
  CBExitGameLoop& operator=(const CBExitGameLoop& o) = delete;
  CBExitGameLoop(CBExitGameLoop&& o) = delete;
  CBExitGameLoop& operator=(CBExitGameLoop&& o) = delete;
  ~CBExitGameLoop() override = default;

  void do_call() override {
    // Delete all callbacks except ourselves
    // NB. the corresponding HookCallback must be removed from Hook object
    // (shared_ptr would be handy here)
    auto [mut, s] = I->aq_storage();
    get_data(I)->sv.mutable_game_state()->set_stage(
        ra2yrproto::ra2yr::STAGE_EXIT_GAME);

    auto [lk, hhooks] = I->aq_hooks();
    auto* callbacks = get_callbacks(I);
    // Loop through all callbacks
    std::vector<std::string> keys;
    std::transform(callbacks->begin(), callbacks->end(),
                   std::back_inserter(keys),
                   [](const auto& v) { return v.first; });

    for (const auto& k : keys) {
      if (k == name()) {
        continue;
      }
      // Get corresponding hook
      auto h = std::find_if(hhooks->begin(), hhooks->end(), [&](auto& a) {
        return (a.second.name() == callbacks->at(k)->target());
      });
      // Remove callback's reference from Hook
      if (h == hhooks->end()) {
        eprintf("no hook found for callback {}", k);
      } else {
        h->second.remove_callback(k);
        // Delete callback object
        callbacks->erase(k);
      }
    }

    // Flush output in case the process is not terminated gracefully.
    std::cerr << std::flush;
    std::cout << std::flush;
  }
};

struct CBUpdateLoadProgress final : public MyCB<CBUpdateLoadProgress> {
  static constexpr char key_name[] = "cb_progress_update";
  static constexpr char key_target[] = "on_progress_update";

  CBUpdateLoadProgress() = default;

  void exec() override {
    auto* B = ProgressScreenClass::Instance().PlayerProgresses;

    auto* sv = &data()->sv;
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
  }
};

struct CBSaveState final : public MyCB<CBSaveState> {
  ra2yrcpp::protocol::MessageOstream out;
  utility::worker_util<std::shared_ptr<ra2yrproto::ra2yr::GameState>> work;
  ra2yrproto::ra2yr::GameState* initial_state;
  std::vector<ra2::Cell> cells;

  static constexpr char key_name[] = "save_state";
  static constexpr char key_target[] = "on_frame_update";

  explicit CBSaveState(std::shared_ptr<std::ostream> record_stream)
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

  void exec() override {
    // enables event debug logs
    // *reinterpret_cast<char*>(0xa8ed74) = 1;
    auto st = state_to_protobuf(type_classes()->empty());
    work.push(st);
  }
};

template <typename D>
struct CBTunnel : public MyCB<D> {
 public:
  using writer_t = std::shared_ptr<ra2yrcpp::protocol::MessageOstream>;

  struct packet_buffer {
    void* data;
    i32 size;  // set to -1 on error
    // these indicate just the packet direction: if receiving, source=1, if
    // sending, destination=1
    u32 source;
    u32 destination;
  };

  writer_t out;

  explicit CBTunnel(writer_t out) : out(out) {}

  void write_packet(const u32 source, const u32 dest, const void* buf,
                    std::size_t len) {
    // dprintf("source={} dest={}, buf={}, len={}", source, dest, buf, len);
    ra2yrproto::ra2yr::TunnelPacket P;
    P.set_source(source);
    P.set_destination(dest);
    P.mutable_data()->assign(static_cast<const char*>(buf), len);
    if (!out->write(P)) {
      throw std::runtime_error(
          fmt::format("{} write_packet failed", D::key_name));
    }
  }

  virtual packet_buffer buffer() = 0;

  void exec() override {
    auto b = buffer();
    if (b.size > 0) {
      write_packet(b.source, b.destination, b.data, b.size);
    }
  }
};

// TODO(shmocz): pass smart ptr by reference?
struct CBTunnelRecvFrom final : public CBTunnel<CBTunnelRecvFrom> {
  static constexpr char key_target[] = "cb_tunnel_recvfrom";
  static constexpr char key_name[] = "tunnel_recvfrom";

  explicit CBTunnelRecvFrom(writer_t out) : CBTunnel(std::move(out)) {}

  packet_buffer buffer() override {
    return {reinterpret_cast<void*>(cpu_state->ebp + 0x3f074),
            static_cast<i32>(cpu_state->esi), 1U, 0U};
  }
};

struct CBTunnelSendTo final : public CBTunnel<CBTunnelSendTo> {
  static constexpr char key_target[] = "cb_tunnel_sendto";
  static constexpr char key_name[] = "tunnel_sendto";

  explicit CBTunnelSendTo(writer_t out) : CBTunnel(std::move(out)) {}

  packet_buffer buffer() override {
    return {reinterpret_cast<void*>(cpu_state->ecx),
            static_cast<i32>(cpu_state->eax), 0U, 1U};
  }
};

struct CBDebugPrint final : public MyCB<CBDebugPrint> {
  static constexpr char key_target[] = "cb_debug_print";
  static constexpr char key_name[] = "debug_print";

  CBDebugPrint() = default;

  // TODO(shmocz): store debug messages in record file
  void exec() override {
    if (configuration()->debug_log()) {
      char buf[1024];
      std::memset(buf, 'F', sizeof(buf));
      abi()->sprintf(reinterpret_cast<char**>(&buf), cpu_state->esp + 0x4);
      fmt::print(stderr, "({}) {}", serialize::read_obj<void*>(cpu_state->esp),
                 buf);
    }
  }
};

ra2yrcpp::hooks_yr::GameDataYR* ra2yrcpp::hooks_yr::get_data(
    ra2yrcpp::InstrumentationService* I) {
  return ensure_storage_value<ra2yrcpp::hooks_yr::GameDataYR>(I, "game_data");
}

// TODO(shmocz): ensure thread safety
void ra2yrcpp::hooks_yr::init_callbacks(ra2yrcpp::hooks_yr::GameDataYR* D) {
  if (D->callbacks_initialized) {
    return;
  }
  auto t = std::to_string(static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count()));
  auto f = [D](std::unique_ptr<ra2yrcpp::ISCallback> c) {
    D->callbacks.try_emplace(c->name(), std::move(c));
  };

  if (std::getenv("RA2YRCPP_RECORD_TRAFFIC") != nullptr) {
    const std::string traffic_out = fmt::format("traffic.{}.pb.gz", t);
    iprintf("record traffic to {}", traffic_out);

    auto out = std::make_shared<ra2yrcpp::protocol::MessageOstream>(
        std::make_shared<std::ofstream>(
            traffic_out, std::ios_base::out | std::ios_base::binary),
        true);
    f(std::make_unique<CBTunnelRecvFrom>(out));
    f(std::make_unique<CBTunnelSendTo>(out));
  }
  f(std::make_unique<CBExitGameLoop>());
  f(std::make_unique<CBGameCommand>());

  std::shared_ptr<std::ofstream> record_out = nullptr;

  if (std::getenv("RA2YRCPP_RECORD_PATH") != nullptr) {
    const std::string record_path = std::getenv("RA2YRCPP_RECORD_PATH");
    D->cfg.set_record_filename(record_path);
    iprintf("record state to {}", record_path);
    record_out = std::make_shared<std::ofstream>(
        record_path, std::ios_base::out | std::ios_base::binary);
  }
  f(std::make_unique<CBSaveState>(record_out));
  f(std::make_unique<CBUpdateLoadProgress>());
  f(std::make_unique<CBDebugPrint>());
}

constexpr std::array<YRHook, 6> gg_hooks = {{
    {0x55de4f, 7U, CBGameCommand::key_target},         //
    {0x72dfb0, 6U, CBExitGameLoop::key_target},        //
    {0x7b3d6f, 6U, CBTunnelSendTo::key_target},        //
    {0x7b3f15, 6U, CBTunnelRecvFrom::key_target},      //
    {0x643c62, 6U, CBUpdateLoadProgress::key_target},  //
    {0x4068e0, 6U, CBDebugPrint::key_target},
}};

std::vector<YRHook> ra2yrcpp::hooks_yr::get_hooks() {
  return std::vector<YRHook>(gg_hooks.begin(), gg_hooks.end());
}
