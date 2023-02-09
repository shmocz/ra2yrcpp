#include "commands_yr.hpp"

using namespace std::chrono_literals;
using google::protobuf::RepeatedPtrField;
using util_command::get_cmd;
using cb_map_t = std::map<std::string, std::unique_ptr<yrclient::ISCallback>>;

// TODO(shmocz): smarter way to define these
// static keys for callbacks, hooks and misc. data
constexpr char key_callbacks_yr[] = "callbacks_yr";
constexpr char key_game_state[] = "game_state";
constexpr char key_object_type_classes[] = "object_type_classes";
constexpr char key_raw_game_state[] = "raw_game_state";
constexpr char key_on_load_game[] = "on_load_game";
constexpr char key_map_data[] = "map_data";

template <typename T, typename... ArgsT>
T* ensure_storage_value(yrclient::InstrumentationService* I,
                        yrclient::storage_t* s, const std::string key,
                        ArgsT... args) {
  if (s->find(key) == s->end()) {
    I->store_value(key, utility::make_uptr<T>(args...));
  }
  return static_cast<T*>(s->at(key).get());
}

static auto default_configuration() {
  ra2yrproto::commands::Configuration C;
  C.set_debug_log(true);
  return C;
}

// FIXME: dont explicitly pass storage
static ra2yrproto::commands::Configuration* ensure_configuration(
    yrclient::InstrumentationService* I, yrclient::storage_t* s) {
  static constexpr char key_configuration[] = "yr_config";
  if (s->find(key_configuration) == s->end()) {
    dprintf("replace config!");
    (void)ensure_storage_value<ra2yrproto::commands::Configuration>(
        I, s, key_configuration, default_configuration());
  }
  return ensure_storage_value<ra2yrproto::commands::Configuration>(
      I, s, key_configuration);
}

struct CBYR : public yrclient::ISCallback {
  yrclient::storage_t* storage{nullptr};
  static constexpr char key_configuration[] = "yr_config";

  CBYR() = default;

  void do_call(yrclient::InstrumentationService* I) override {
    auto [mut, s] = I->aq_storage();
    storage = s;
    try {
      exec();
    } catch (const std::exception& e) {
      eprintf("FATAL: {}", e.what());
    }
    storage = nullptr;
  }

  template <typename T>
  auto* storage_value(const std::string key) {
    return ensure_storage_value<T>(this->I, this->storage, key);
  }

  auto* game_state() {
    return storage_value<ra2yrproto::ra2yr::GameState>(key_game_state);
  }

  auto* type_classes() {
    return storage_value<RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>>(
        key_object_type_classes);
  }

  auto* abi() { return storage_value<ra2::abi::ABIGameMD>("abi"); }

  virtual void exec() { throw std::runtime_error("Not implemented"); }

  ra2yrproto::commands::Configuration* configuration() {
    return ensure_configuration(this->I, this->storage);
  }
};

static auto* get_callbacks(yrclient::InstrumentationService* I,
                           const bool acquire = false) {
  return utility::asptr<cb_map_t*>(
      utility::asint(I->get_value(key_callbacks_yr, acquire)));
}

// Combining CRTP with polymorphism.
template <typename D, typename B = CBYR>
struct MyCB : public B {
  std::string name() override { return D::key_name; }

  std::string target() override { return D::key_target; }
};

struct CBExitGameLoop : public MyCB<CBExitGameLoop, yrclient::ISCallback> {
  static constexpr char key_target[] = "on_gameloop_exit";
  static constexpr char key_name[] = "gameloop_exit";

  CBExitGameLoop() = default;
  CBExitGameLoop(const CBExitGameLoop& o) = delete;
  CBExitGameLoop& operator=(const CBExitGameLoop& o) = delete;
  CBExitGameLoop(CBExitGameLoop&& o) = delete;
  CBExitGameLoop& operator=(CBExitGameLoop&& o) = delete;
  ~CBExitGameLoop() override = default;

  void do_call(yrclient::InstrumentationService* I) override {
    // Delete all callbacks except ourselves
    // NB. the corresponding HookCallback must be removed from Hook object
    // (shared_ptr would be handy here)
    auto [mut, s] = I->aq_storage();
    ensure_storage_value<ra2yrproto::ra2yr::GameState>(I, s, key_game_state)
        ->set_stage(ra2yrproto::ra2yr::STAGE_EXIT_GAME);

    auto [lk, hhooks] = I->aq_hooks();
    auto* callbacks = get_callbacks(I, false);
    // Loop through all callbacks
    std::vector<std::string> keys;
    std::transform(callbacks->begin(), callbacks->end(),
                   std::back_inserter(keys),
                   [](const auto& v) { return v.first; });
    for (const auto& k : keys) {
      if (k != name()) {
        // Get corresponding hook
        auto h = std::find_if(hhooks->begin(), hhooks->end(), [&](auto& a) {
          return (a.second.name() == callbacks->at(k)->target());
        });
        // Remove callback's reference from Hook
        if (h == hhooks->end()) {
          throw std::runtime_error("hook not found");
        }
        h->second.remove_callback(k);
        // Delete callback object
        callbacks->erase(k);
      }
    }
  }
};

struct CBUpdateLoadProgress : public MyCB<CBUpdateLoadProgress> {
  static constexpr char key_state[] = "progress";
  static constexpr char key_name[] = "cb_progress_update";
  static constexpr char key_target[] = "on_progress_update";

  CBUpdateLoadProgress() = default;

  void exec() override {
    // this = ESI
    auto* P = reinterpret_cast<ProgressScreenClass*>(cpu_state->esi);
    auto* B = P->PlayerProgresses;
    auto* local_state = storage_value<ra2yrproto::ra2yr::GameState>(key_state);
    if (local_state->load_progresses().empty()) {
      for (auto i = 0U; i < (sizeof(*B) / sizeof(B)); i++) {
        local_state->add_load_progresses(0.0);
      }
    }
    for (int i = 0; i < local_state->load_progresses().size(); i++) {
      local_state->set_load_progresses(i, B[i]);
    }
    storage_value<ra2yrproto::ra2yr::GameState>(key_game_state)
        ->set_stage(ra2yrproto::ra2yr::LoadStage::STAGE_LOADING);
  }
};

struct entry {
  std::shared_ptr<void> data;
  std::function<void(CBYR*, void*)> fn;
};

struct CBExecuteGameLoopCommand : public MyCB<CBExecuteGameLoopCommand> {
  static constexpr char key_name[] = "cb_execute_gameloop_command";
  static constexpr char key_target[] = "on_frame_update";
  async_queue::AsyncQueue<entry> work;

  CBExecuteGameLoopCommand() = default;

  void exec() override {
    auto items = work.pop(0, 0ms);
    for (const auto& it : items) {
      it.fn(this, it.data.get());
    }
  }
};

struct CBSaveState : public MyCB<CBSaveState> {
  static constexpr char key_name[] = "save_state";
  static constexpr char key_target[] = "on_frame_update";
  static constexpr char key_record_path[] = "record_path";
  const std::string record_path;

  std::unique_ptr<yrclient::CompressedOutputStream> out;
  utility::worker_util<ra2yrproto::ra2yr::GameState> work;

  explicit CBSaveState(const std::string record_path)
      : record_path(record_path),
        out(std::make_unique<yrclient::CompressedOutputStream>(record_path)),
        work([this](ra2yrproto::ra2yr::GameState& w) {
          this->serialize_state(w);
        }) {}

  void serialize_state(const ra2yrproto::ra2yr::GameState& G) const {
    if (out != nullptr) {
      google::protobuf::io::CodedOutputStream co(&out->s_g);

      if (!yrclient::write_message(&G, &co)) {
        throw std::runtime_error("write_message");
      }
    }
  }

  void parse_Objects(ra2yrproto::ra2yr::GameState* G) {
    auto* D = TechnoClass::Array.get();
    auto* H = G->mutable_objects();
    if (H->size() != D->Count) {
      H->Clear();
      for (int i = 0; i < D->Count; i++) {
        H->Add();
      }
    }
    for (int i = 0; i < D->Count; i++) {
      auto* I = D->Items[i];
      auto& O = H->at(i);
      ra2::ClassParser P({abi(), I}, &O);
      P.parse();
    }
  }

  void parse_Factories(ra2yrproto::ra2yr::GameState* G) {
    auto* D = FactoryClass::Array.get();
    auto* H = G->mutable_factories();
    if (H->size() != D->Count) {
      H->Clear();
      for (int i = 0; i < D->Count; i++) {
        H->Add();
      }
    }

    for (int i = 0; i < D->Count; i++) {
      auto* I = D->Items[i];
      auto& O = H->at(i);
      O.set_object(reinterpret_cast<u32>(I->Object));
      O.set_owner(reinterpret_cast<u32>(I->Owner));
      O.set_progress_timer(I->Production.Value);
      O.set_on_hold(I->OnHold);
      auto A = utility::ArrayIterator(I->QueuedObjects.Items,
                                      I->QueuedObjects.Count);
      for (auto* p : A) {
        O.add_queued_objects(reinterpret_cast<u32>(p));
      }
    }
  }

  void parse_HouseClasses(ra2yrproto::ra2yr::GameState* G) {
    auto* D = HouseClass::Array.get();
    auto* H = G->mutable_houses();
    if (H->size() != D->Count) {
      H->Clear();
      for (int i = 0; i < D->Count; i++) {
        H->Add();
      }
    }
    for (int i = 0; i < D->Count; i++) {
      auto* I = D->Items[i];
      auto& O = H->at(i);
      O.set_array_index(I->ArrayIndex);
      O.set_current_player(I->IsInPlayerControl);
      O.set_defeated(I->Defeated);
      O.set_is_game_over(I->IsGameOver);
      O.set_is_loser(I->IsLoser);
      O.set_is_winner(I->IsWinner);
      O.set_money(I->Balance);
      O.set_power_drain(I->PowerDrain);
      O.set_power_output(I->PowerOutput);
      O.set_start_credits(I->StartingCredits);
      O.set_self(reinterpret_cast<std::uintptr_t>(I));
      O.set_name(I->PlainName);
    }
  }

  void parse_AbstractTypeClasses(
      RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>* T) {
    auto* D = AbstractTypeClass::Array.get();

    // Initialize if no types haven't been parsed yet
    if (T->size() != D->Count) {
      T->Clear();
      for (int i = 0; i < D->Count; i++) {
        T->Add();
      }
    }

    // Parse the types
    for (int i = 0; i < D->Count; i++) {
      auto* I = D->Items[i];
      auto& A = T->at(i);
      // TODO: UB?
      ra2::TypeClassParser P({abi(), I}, &A);
      P.parse();
    }
  }

  ra2yrproto::ra2yr::GameState state_to_protobuf(
      const bool do_type_classes = false) {
    auto* gbuf = storage_value<ra2yrproto::ra2yr::GameState>(key_game_state);
    gbuf->Clear();

    // put load stages
    gbuf->mutable_load_progresses()->CopyFrom(
        storage_value<ra2yrproto::ra2yr::GameState>(
            CBUpdateLoadProgress::key_state)
            ->load_progresses());

    // Parse type classes only once
    if (do_type_classes) {
      parse_AbstractTypeClasses(type_classes());
      gbuf->mutable_object_types()->CopyFrom(*type_classes());
    }

    parse_HouseClasses(gbuf);
    parse_Objects(gbuf);
    parse_Factories(gbuf);

    // At this point we're free to leave the callback
    gbuf->set_stage(ra2yrproto::ra2yr::LoadStage::STAGE_INGAME);
    gbuf->set_current_frame(Unsorted::CurrentFrame);

    if (gbuf->current_frame() > 0U) {
      auto* M = MapClass::Instance.get();
      auto L = M->MapCoordBounds;
      auto sz = (L.Right + 1) * (L.Bottom + 1);
      auto* m = storage_value<ra2yrproto::ra2yr::MapData>(key_map_data)
                    ->mutable_cells();
      if (m->size() != sz) {
        m->Clear();
        for (int i = 0; i < sz; i++) {
          m->Add();
        }
      }
      for (int i = 0; i <= L.Right; i++) {
        for (int j = 0; j <= L.Bottom; j++) {
          CellStruct coords{static_cast<i16>(i), static_cast<i16>(j)};
          auto* src_cell = M->TryGetCellAt(coords);

          if (src_cell != nullptr) {
            auto& c = m->at((L.Bottom + 1) * j + i);
            c.set_land_type(
                static_cast<ra2yrproto::ra2yr::LandType>(src_cell->LandType));
            c.set_height(src_cell->Height);
            c.set_level(src_cell->Level);
            c.set_radiation_level(src_cell->RadLevel);
            c.set_overlay_data(src_cell->OverlayData);
            if (src_cell->FirstObject != nullptr) {
              c.mutable_objects()->Clear();
              auto* o = c.add_objects();
              o->set_pointer_self(reinterpret_cast<u32>(src_cell->FirstObject));
            }
            if (c.land_type() ==
                ra2yrproto::ra2yr::LandType::LAND_TYPE_Tiberium) {
              c.set_tiberium_value(abi()->CellClass_GetContainedTiberiumValue(
                  reinterpret_cast<std::uintptr_t>(src_cell)));
            }
          }
        }
      }
    }

    return {*gbuf};
  }

  void exec() override {
    // FIXME: more explicit about this
    // enables event debug logs
    // *reinterpret_cast<char*>(0xa8ed74) = 1;
    auto st = state_to_protobuf(type_classes()->empty());
    work.push(st);
  }
};

template <typename D>
struct CBTunnel : public MyCB<D> {
 public:
  struct packet_buffer {
    void* data;
    i32 size;  // set to -1 on error
    // these indicate just the packet direction: if receiving, source=1, if
    // sending, destination=1
    u32 source;
    u32 destination;
  };

  std::shared_ptr<yrclient::CompressedOutputStream> out;

  explicit CBTunnel(std::shared_ptr<yrclient::CompressedOutputStream> out)
      : out(std::move(out)) {}

  void write_packet(const u32 source, const u32 dest, const void* buf,
                    size_t len) {
    dprintf("source={} dest={}, buf={}, len={}", source, dest, buf, len);
    ra2yrproto::ra2yr::TunnelPacket P;
    google::protobuf::io::CodedOutputStream co(&out->s_g);
    P.set_source(source);
    P.set_destination(dest);
    P.mutable_data()->assign(static_cast<const char*>(buf), len);
    yrclient::write_message(&P, &co);
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
struct CBTunnelRecvFrom : public CBTunnel<CBTunnelRecvFrom> {
  static constexpr char key_target[] = "cb_tunnel_recvfrom";
  static constexpr char key_name[] = "tunnel_recvfrom";

  explicit CBTunnelRecvFrom(
      std::shared_ptr<yrclient::CompressedOutputStream> out)
      : CBTunnel(std::move(out)) {}

  packet_buffer buffer() override {
    return {utility::asptr(cpu_state->ebp + 0x3f074),
            static_cast<i32>(cpu_state->esi), 1U, 0U};
  }
};

struct CBTunnelSendTo : public CBTunnel<CBTunnelSendTo> {
  static constexpr char key_target[] = "cb_tunnel_sendto";
  static constexpr char key_name[] = "tunnel_sendto";

  explicit CBTunnelSendTo(std::shared_ptr<yrclient::CompressedOutputStream> out)
      : CBTunnel(std::move(out)) {}

  packet_buffer buffer() override {
    return {utility::asptr(cpu_state->ecx), static_cast<i32>(cpu_state->eax),
            0U, 1U};
  }
};

struct CBDebugPrint : public MyCB<CBDebugPrint> {
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

// FIXME: ensure proper locking
static void init_callbacks(yrclient::InstrumentationService* I) {
  I->store_value(key_callbacks_yr, utility::make_uptr<cb_map_t>());

  auto t = std::to_string(static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count()));

  auto f = [I](std::unique_ptr<yrclient::ISCallback> c) {
    get_callbacks(I)->try_emplace(c->name(), std::move(c));
  };

  // TODO(shmocz): customizable output path
  const std::string traffic_out = fmt::format("traffic.{}.pb.gz", t);
  const std::string record_out = fmt::format("record.{}.pb.gz", t);
  std::shared_ptr<yrclient::CompressedOutputStream> out =
      std::make_shared<yrclient::CompressedOutputStream>(traffic_out);

  ensure_configuration(I, &I->storage())->set_record_filename(record_out);

  f(std::make_unique<CBTunnelRecvFrom>(out));
  f(std::make_unique<CBTunnelSendTo>(out));
  f(std::make_unique<CBSaveState>(record_out));
  f(std::make_unique<CBExitGameLoop>());
  f(std::make_unique<CBExecuteGameLoopCommand>());
  f(std::make_unique<CBUpdateLoadProgress>());
  f(std::make_unique<CBDebugPrint>());
}

// FIXME: don't allow deploying of already deployed object
static void unit_action(const u32 p_object,
                        const ra2yrproto::commands::UnitAction a,
                        ra2::abi::ABIGameMD* abi) {
  using ra2yrproto::commands::UnitAction;
  switch (a) {
    case UnitAction::ACTION_DEPLOY:
      abi->DeployObject(p_object);  // NB. doesn't work online
      break;
    case UnitAction::ACTION_SELL:
      abi->SellBuilding(p_object);
      break;
    case UnitAction::ACTION_SELECT:
#ifdef _MSC_VER
      reinterpret_cast<ObjectClass*>(p_object)->Select();
#else
      (void)abi->SelectObject(p_object);
#endif
      break;
    default:
      break;
  }
}

template <typename T>
static auto* get_callback(yrclient::InstrumentationService* I) {
  return utility::asptr<T*>(
      utility::asint(get_callbacks(I)->at(T::key_name).get()));
}

template <typename ArgT>
entry make_work(const ArgT& aa, std::function<void(CBYR*, ArgT*)> fn) {
  return {utility::make_sptr<ArgT>(aa),
          [fn](CBYR* C, void* data) { fn(C, static_cast<ArgT*>(data)); }};
}

namespace cmd {
auto click_event() {
  return get_cmd<ra2yrproto::commands::ClickEvent>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto a = Q->args();

    get_callback<CBExecuteGameLoopCommand>(Q->I())->work.push(
        make_work<decltype(a)>(a, [](CBYR* C, auto* args) {
          auto& O = C->game_state()->objects();
          for (auto k : args->object_addresses()) {
            if (std::find_if(O.begin(), O.end(), [k](auto& v) {
                  return v.pointer_self() == k;
                }) != O.end()) {
              dprintf("clickevent {} {}", k, static_cast<int>(args->event()));
              (void)C->abi()->ClickEvent(k, args->event());
            }
          }
        }));
  });
}

auto unit_command() {
  return get_cmd<ra2yrproto::commands::UnitCommand>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto a = Q->args();

    get_callback<CBExecuteGameLoopCommand>(Q->I())->work.push(
        make_work<decltype(a)>(a, [](CBYR* C, auto* args) {
          auto& O = C->game_state()->objects();
          for (auto k : args->object_addresses()) {
            if (std::find_if(O.begin(), O.end(), [k](auto& v) {
                  return v.pointer_self() == k;
                }) != O.end()) {
              unit_action(k, args->action(), C->abi());
            }
          }
        }));
  });
}

auto create_callbacks() {
  return get_cmd<ra2yrproto::commands::CreateCallbacks>([](auto* Q) {
    auto [lk_s, s] = Q->I()->aq_storage();
    // Create ABI
    (void)ensure_storage_value<ra2::abi::ABIGameMD>(Q->I(), s, "abi");

    if (s->find(key_callbacks_yr) == s->end()) {
      init_callbacks(Q->I());
    }
    auto cbs = get_callbacks(Q->I());
    lk_s.unlock();
    auto [lk, hhooks] = Q->I()->aq_hooks();
    for (auto& [k, v] : *cbs) {
      auto target = v->target();
      auto h = std::find_if(hhooks->begin(), hhooks->end(), [&](auto& a) {
        return (a.second.name() == target);
      });
      if (h == hhooks->end()) {
        throw yrclient::general_error(fmt::format("No such hook {}", target));
      }

      const std::string hook_name = k;
      auto& tmp_cbs = h->second.callbacks();
      // TODO(shmocz): throw standard exception
      if (std::find_if(tmp_cbs.begin(), tmp_cbs.end(), [&hook_name](auto& a) {
            return a.name == hook_name;
          }) != tmp_cbs.end()) {
        throw yrclient::general_error(fmt::format(
            "Hook {} already has a callback {}", target, hook_name));
      }

      dprintf("add hook, target={} cb={}", target, hook_name);
      auto cb = v.get();
      // FIXME: avoid using wrapper
      h->second.add_callback(
          [cb](hook::Hook* h, void* user_data, X86Regs* state) {
            cb->call(h, user_data, state);
          },
          Q->I(), k, 0U);
    }
  });
}

constexpr std::array<std::pair<const char*, u32>, 7> gg_hooks = {{
    {CBExecuteGameLoopCommand::key_target, 0x55de7f},  //
    {CBExitGameLoop::key_target, 0x72dfb0},            //
    {key_on_load_game, 0x686730},                      //
    {CBTunnelSendTo::key_target, 0x7b3d6f},            //
    {CBTunnelRecvFrom::key_target, 0x7b3f15},          //
    {CBUpdateLoadProgress::key_target, 0x643c62},      //
    {CBDebugPrint::key_target, 0x4068e0},              //
}};

auto create_hooks() {
  return get_cmd<ra2yrproto::commands::CreateHooks>([](auto* Q) {
    // TODO(shmocz): put these to utility function and share code with Hook
    // code.
    // suspend threads
    auto P = process::get_current_process();
    std::vector<process::thread_id_t> ns(Q->I()->get_connection_threads());
    ns.push_back(process::get_current_tid());
    P.suspend_threads(ns);

    // create hooks
    for (const auto& [k, v] : gg_hooks) {
      auto [p_target, code_size] = hook::get_hook_entry(v);
      Q->I()->create_hook(k, utility::asptr<u8*>(p_target), code_size);
    }
    P.resume_threads(ns);
  });
}

auto get_game_state() {
  return get_cmd<ra2yrproto::commands::GetGameState>([](auto* Q) {
    // Copy saved game state
    auto [mut, s] = Q->I()->aq_storage();
    Q->command_data().mutable_result()->mutable_state()->CopyFrom(
        *ensure_storage_value<ra2yrproto::ra2yr::GameState>(Q->I(), s,
                                                            key_game_state));
  });
}

auto get_type_classes() {
  return get_cmd<ra2yrproto::commands::GetTypeClasses>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto res = Q->command_data().mutable_result();
    res->mutable_classes()->CopyFrom(
        *ensure_storage_value<
            RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>>(
            Q->I(), s, key_object_type_classes));
  });
}

// FIXME: setting values not working
auto inspect_configuration() {
  return get_cmd<ra2yrproto::commands::InspectConfiguration>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto res = Q->command_data().mutable_result();
    res->mutable_config()->CopyFrom(
        *ensure_storage_value<ra2yrproto::commands::Configuration>(
            Q->I(), s, CBYR::key_configuration));
  });
}

// NB. CellClicked not called for moving units, but for attack (and what
// else?) ClickedMission seems to be used for various other events
// FIXME: need to implement GetCellAt(coords)
auto mission_clicked() {
  return get_cmd<ra2yrproto::commands::MissionClicked>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto a = Q->args();

    get_callback<CBExecuteGameLoopCommand>(Q->I())->work.push(
        make_work<decltype(a)>(a, [](CBYR* C, auto* args) {
          auto& O = C->game_state()->objects();
          for (auto k : args->object_addresses()) {
            if (std::find_if(O.begin(), O.end(), [k](auto& v) {
                  return v.pointer_self() == k;
                }) != O.end()) {
              auto c1 = args->coordinates();
              auto coords = CoordStruct{.X = c1.x(), .Y = c1.y(), .Z = c1.z()};
              auto cell = MapClass::Instance.get()->TryGetCellAt(coords);
              auto* p = reinterpret_cast<CellClass*>(cell);
              C->abi()->ClickedMission(k, static_cast<Mission>(args->event()),
                                       args->target_object(), p, p);
            }
          }
        }));
  });
}

auto add_event() {
  return get_cmd<ra2yrproto::commands::AddEvent>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto a = Q->args();
    // FIXME: UAF?

    get_callback<CBExecuteGameLoopCommand>(Q->I())->work.push(
        make_work<decltype(a)>(a, [](CBYR* C, auto* args) {
          // TODO: these may not be needed
          EventClass E(static_cast<EventType>(args->event().event_type()),
                       false, static_cast<char>(args->event().house_index()),
                       static_cast<u32>(Unsorted::CurrentFrame));
          if (args->event().has_production()) {
            auto& ev = args->event().production();
            E.Data.Production = {.RTTI_ID = ev.rtti_id(),
                                 .Heap_ID = ev.heap_id(),
                                 .IsNaval = ev.is_naval()};
            auto ts = static_cast<int>(C->abi()->timeGetTime());
            if (!EventClass::AddEvent(E, ts)) {
              throw std::runtime_error("failed to add event");
            }
          } else if (args->event().has_place()) {
            auto& ev = args->event().place();
            auto loc = ev.location();
            auto S = CoordStruct{.X = loc.x(), .Y = loc.y(), .Z = loc.z()};
            E.Data.Place = {
                .RTTIType = static_cast<AbstractType>(ev.rtti_type()),
                .HeapID = ev.heap_id(),
                .IsNaval = ev.is_naval(),
                .Location = CellClass::Coord2Cell(S)};
            (void)EventClass::AddEvent(E, C->abi()->timeGetTime());
          } else {
            // generic event
            dprintf("generic,house={}", args->event().house_index());
            (void)EventClass::AddEvent(E, C->abi()->timeGetTime());
          }
        }));
  });
}

auto place_query() {
  return get_cmd<ra2yrproto::commands::PlaceQuery>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto a = Q->args();
    auto cmd = Q->c;
    Q->save_command_result();
    cmd->pending().store(true);

    get_callback<CBExecuteGameLoopCommand>(Q->I())->work.push(
        make_work<decltype(a)>(a, [cmd](CBYR* C, auto* args) {
          // Get corresponding BuildingTypeClass
          auto A1 = TechnoTypeClass::Array.get();
          auto A =
              utility::ArrayIterator<TechnoTypeClass*>(A1->Items, A1->Count);
          auto B = std::find_if(A.begin(), A.end(), [args](auto* p) {
            return utility::asint(p) == args->type_class();
          });
          // Get HouseClass
          auto& H = C->game_state()->houses();

          // TODO(shmocz): make helper method
          auto house = std::find_if(H.begin(), H.end(), [](const auto& h) {
            return h.current_player();
          });
          if (args->house_class()) {
            house = std::find_if(H.begin(), H.end(), [args](const auto& h) {
              return h.self() == args->house_class();
            });
          }

          ra2yrproto::commands::PlaceQuery r2;

          auto* p = reinterpret_cast<ra2yrproto::CommandResult*>(cmd->result());
          p->mutable_result()->UnpackTo(&r2);
          auto r2_res = r2.mutable_result();

          // Call for each cell
          if (B != A.end()) {
            auto* q = static_cast<BuildingTypeClass*>(*B);
            for (auto& c : args->coordinates()) {
              auto coords = CoordStruct{.X = c.x(), .Y = c.y(), .Z = c.z()};
              auto cell_s = CellClass::Coord2Cell(coords);
              auto* cs = reinterpret_cast<CellStruct*>(&cell_s);

              auto p_DisplayClass = 0x87F7E8u;
              // FIXME. properly get house index!
              // FIXME: rename BuildingClass to BuildingTypeClass
              if (C->abi()->DisplayClass_Passes_Proximity_Check(
                      p_DisplayClass, reinterpret_cast<BuildingTypeClass*>(q),
                      0u, cs) &&
                  C->abi()->BuildingClass_CanPlaceHere(utility::asint(q), cs,
                                                       house->self())) {
                auto* cnew = r2_res->add_coordinates();
                cnew->CopyFrom(c);
              }
            }
          } else {
            eprintf("could not locate building tc");
          }
          // copy results
          p->mutable_result()->PackFrom(r2);

          cmd->pending().store(false);
        }));
  });
}

auto send_message() {
  return get_cmd<ra2yrproto::commands::AddMessage>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto a = Q->args();

    get_callback<CBExecuteGameLoopCommand>(Q->I())->work.push(
        make_work<decltype(a)>(a, [](CBYR* C, auto* args) {
          C->abi()->AddMessage(1, args->message(), args->color(), 0x4046,
                               args->duration_frames(), false);
        }));
  });
}

auto read_value() {
  return get_cmd<ra2yrproto::commands::ReadValue>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto a = Q->args();
    auto* D = Q->command_data().mutable_result()->mutable_data();
    if (a.data().has_game_state()) {
      D->mutable_game_state()->CopyFrom(
          *ensure_storage_value<ra2yrproto::ra2yr::GameState>(Q->I(), s,
                                                              key_game_state));
    } else if (a.data().has_map_data()) {
      D->mutable_map_data()->CopyFrom(
          *ensure_storage_value<ra2yrproto::ra2yr::MapData>(Q->I(), s,
                                                            key_map_data));
    } else {
      throw std::runtime_error("invalid target");
    }
  });
}

}  // namespace cmd

std::map<std::string, command::Command::handler_t> commands_yr::get_commands() {
  return {cmd::click_event(),            //
          cmd::unit_command(),           //
          cmd::create_callbacks(),       //
          cmd::create_hooks(),           //
          cmd::get_game_state(),         //
          cmd::get_type_classes(),       //
          cmd::inspect_configuration(),  //
          cmd::mission_clicked(),        //
          cmd::add_event(),              //
          cmd::place_query(),            //
          cmd::send_message(),           //
          cmd::read_value()};
}
