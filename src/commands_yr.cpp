#include "commands_yr.hpp"

using namespace std::chrono_literals;
using google::protobuf::RepeatedPtrField;
using util_command::get_cmd;
using cb_map_t = std::map<std::string, std::unique_ptr<yrclient::ISCallback>>;

// static keys for callbacks, hooks and misc. data
constexpr char key_callbacks_yr[] = "callbacks_yr";
constexpr char key_on_load_game[] = "on_load_game";

// TODO: reduce calls to this
template <typename T, typename... ArgsT>
T* ensure_storage_value(yrclient::InstrumentationService* I,
                        yrclient::storage_t* s, const std::string key,
                        ArgsT... args) {
  if (s->find(key) == s->end()) {
    I->store_value(key, utility::make_uptr<T>(args...));
  }
  return static_cast<T*>(s->at(key).get());
}

auto* get_storage(yrclient::InstrumentationService* I, yrclient::storage_t* s) {
  return ensure_storage_value<ra2yrproto::commands::StorageValue>(
      I, s, "message_storage");
}

static auto default_configuration() {
  ra2yrproto::commands::Configuration C;
  C.set_debug_log(true);
  C.set_parse_map_data_interval(1U);
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

// TODO: specify name of taks in make_work to aid diagnostics
struct CBYR : public yrclient::ISCallback {
  yrclient::storage_t* storage{nullptr};
  ra2::abi::ABIGameMD* abi_{nullptr};
  ra2yrproto::commands::Configuration* config_{nullptr};
  static constexpr char key_configuration[] = "yr_config";

  CBYR() = default;

  template <typename T>
  auto* storage_value(const std::string key) {
    return ensure_storage_value<T>(this->I, this->storage, key);
  }

  template <typename T>
  auto* storage_value() {
    return ensure_storage_value<typename T::type>(this->I, this->storage,
                                                  T::key);
  }

  auto* abi() {
    if (abi_ != nullptr) {
      return abi_;
    }
    return storage_value<ra2::abi::ABIGameMD>("abi");
  }

  void do_call(yrclient::InstrumentationService* I) override {
    auto [mut, s] = I->aq_storage();
    storage = s;
    abi_ = abi();
    auto [mut_cc, cc] = abi_->acquire_code_generators();
    try {
      exec();
    } catch (const std::exception& e) {
      eprintf("{}: {}", name(), e.what());
    }
    storage = nullptr;
  }

  auto* game_state() {
    return get_storage(this->I, this->storage)->mutable_game_state();
  }

  auto* type_classes() {
    return get_storage(this->I, this->storage)
        ->mutable_initial_game_state()
        ->mutable_object_types();
  }

  auto* prerequisite_groups() {
    return get_storage(this->I, this->storage)
        ->mutable_initial_game_state()
        ->mutable_prerequisite_groups();
  }

  virtual void exec() { throw std::runtime_error("Not implemented"); }

  ra2yrproto::commands::Configuration* configuration() {
    if (config_ == nullptr) {
      config_ = ensure_configuration(this->I, this->storage);
    }
    return config_;
  }
};

static auto* get_callbacks(yrclient::InstrumentationService* I,
                           const bool acquire = false) {
  return reinterpret_cast<cb_map_t*>(reinterpret_cast<std::uintptr_t>(
      I->get_value(key_callbacks_yr, acquire)));
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
    get_storage(I, s)->mutable_game_state()->set_stage(
        ra2yrproto::ra2yr::STAGE_EXIT_GAME);

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
    get_storage(I, storage)
        ->mutable_game_state()
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
  const std::string record_path;
  std::uint64_t fps_last_checked;
  std::uint64_t frame_previous;

  std::unique_ptr<yrclient::CompressedOutputStream> out;
  utility::worker_util<std::shared_ptr<ra2yrproto::ra2yr::GameState>> work;
  std::vector<ra2::Cell> cells;

  explicit CBSaveState(const std::string record_path)
      : record_path(record_path),
        fps_last_checked(0U),
        frame_previous(0U),
        out(std::make_unique<yrclient::CompressedOutputStream>(record_path)),
        work([this](const auto& w) { this->serialize_state(*w.get()); }, 10U) {}

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
      yrclient::fill_repeated_empty(H, D->Count);
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
      yrclient::fill_repeated_empty(H, D->Count);
    }

    for (int i = 0; i < D->Count; i++) {
      auto* I = D->Items[i];
      auto& O = H->at(i);
      O.set_object(reinterpret_cast<u32>(I->Object));
      O.set_owner(reinterpret_cast<u32>(I->Owner));
      O.set_progress_timer(I->Production.Value);
      O.set_on_hold(I->OnHold);
      auto A = ra2::abi::DVCIterator(&I->QueuedObjects);
      O.clear_queued_objects();
      for (auto* p : A) {
        O.add_queued_objects(reinterpret_cast<u32>(p));
      }
    }
  }

  void parse_HouseClasses(ra2yrproto::ra2yr::GameState* G) {
    auto* D = HouseClass::Array.get();
    auto* H = G->mutable_houses();
    if (H->size() != D->Count) {
      yrclient::fill_repeated_empty(H, D->Count);
    }

    for (int i = 0; i < D->Count; i++) {
      auto* I = D->Items[i];
      auto& O = H->at(i);
      ra2::parse_HouseClass(&O, I);
    }
  }

  void parse_AbstractTypeClasses(
      RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>* T) {
    auto* D = AbstractTypeClass::Array.get();

    // Initialize if no types haven't been parsed yet
    if (T->size() != D->Count) {
      yrclient::fill_repeated_empty(T, D->Count);
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

  void update_MapData(
      ra2yrproto::ra2yr::MapData* M,
      const RepeatedPtrField<ra2yrproto::ra2yr::Cell>& difference) {
    for (const auto& c : difference) {
      M->mutable_cells()->at(c.index()).CopyFrom(c);
    }
  }

  std::shared_ptr<ra2yrproto::ra2yr::GameState> state_to_protobuf(
      const bool do_type_classes = false) {
    auto* gbuf = get_storage(I, storage)->mutable_game_state();

    // put load stages
    gbuf->mutable_load_progresses()->CopyFrom(
        storage_value<ra2yrproto::ra2yr::GameState>(
            CBUpdateLoadProgress::key_state)
            ->load_progresses());

    // Parse type classes only once
    if (do_type_classes) {
      parse_AbstractTypeClasses(type_classes());
      ra2::parse_prerequisiteGroups(prerequisite_groups());
      gbuf->mutable_object_types()->CopyFrom(*type_classes());
      gbuf->mutable_prerequisite_groups()->CopyFrom(*prerequisite_groups());
    } else {
      gbuf->clear_object_types();
      gbuf->clear_prerequisite_groups();
    }

    gbuf->set_current_frame(Unsorted::CurrentFrame);
    gbuf->set_tech_level(Game::TechLevel);
    parse_HouseClasses(gbuf);
    parse_Objects(gbuf);
    parse_Factories(gbuf);

    gbuf->set_stage(ra2yrproto::ra2yr::LoadStage::STAGE_INGAME);

    // Initialize MapData
    if (gbuf->current_frame() > 0U &&
        get_storage(I, storage)->map_data().cells_size() == 0U) {
      ra2::parse_MapData(get_storage(I, storage)->mutable_map_data(),
                         MapClass::Instance.get(), abi());
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
      update_MapData(get_storage(I, storage)->mutable_map_data(),
                     gbuf->cells_difference());
    }

    if (do_type_classes) {
      auto* gbuf_initial =
          get_storage(I, storage)->mutable_initial_game_state();
      gbuf_initial->CopyFrom(*gbuf);
    }

    ra2::parse_EventLists(gbuf, get_storage(I, storage)->mutable_event_buffer(),
                          cfg::EVENT_BUFFER_SIZE);

    return std::make_shared<ra2yrproto::ra2yr::GameState>(*gbuf);
  }

  void exec() override {
    // enables event debug logs
    // *reinterpret_cast<char*>(0xa8ed74) = 1;
    try {
      auto st = state_to_protobuf(type_classes()->empty());
      work.push(st);
    } catch (const std::exception& e) {
      eprintf("FAILED {}", e.what());
      throw;
    }
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
    // dprintf("source={} dest={}, buf={}, len={}", source, dest, buf, len);
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
    return {reinterpret_cast<void*>(cpu_state->ebp + 0x3f074),
            static_cast<i32>(cpu_state->esi), 1U, 0U};
  }
};

struct CBTunnelSendTo : public CBTunnel<CBTunnelSendTo> {
  static constexpr char key_target[] = "cb_tunnel_sendto";
  static constexpr char key_name[] = "tunnel_sendto";

  explicit CBTunnelSendTo(std::shared_ptr<yrclient::CompressedOutputStream> out)
      : CBTunnel(std::move(out)) {}

  packet_buffer buffer() override {
    return {reinterpret_cast<void*>(cpu_state->ecx),
            static_cast<i32>(cpu_state->eax), 0U, 1U};
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
  f(std::make_unique<CBExitGameLoop>());
  f(std::make_unique<CBExecuteGameLoopCommand>());
  f(std::make_unique<CBSaveState>(record_out));
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
  return reinterpret_cast<T*>(
      reinterpret_cast<u32>(get_callbacks(I)->at(T::key_name).get()));
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
    {CBExecuteGameLoopCommand::key_target, 0x55de4f},  //
    {CBExitGameLoop::key_target, 0x72dfb0},            //
    {key_on_load_game, 0x686730},                      //
    {CBTunnelSendTo::key_target, 0x7b3d6f},            //
    {CBTunnelRecvFrom::key_target, 0x7b3f15},          //
    {CBUpdateLoadProgress::key_target, 0x643c62},      //
    {CBDebugPrint::key_target, 0x4068e0},
}};

auto create_hooks() {
  return get_cmd<ra2yrproto::commands::CreateHooks>([](auto* Q) {
    // TODO(shmocz): put these to utility function and share code with Hook
    // code.
    auto P = process::get_current_process();
    std::vector<process::thread_id_t> ns(Q->I()->get_connection_threads());

    auto a = Q->args();

    // suspend threads?
    if (!a.no_suspend_threads()) {
      ns.push_back(process::get_current_tid());
      P.suspend_threads(ns);
    }

    // create hooks
    for (const auto& [k, v] : gg_hooks) {
      auto [p_target, code_size] = hook::get_hook_entry(v);
      Q->I()->create_hook(k, reinterpret_cast<u8*>(p_target), code_size);
    }
    if (!a.no_suspend_threads()) {
      P.resume_threads(ns);
    }
  });
}

auto get_game_state() {
  return get_cmd<ra2yrproto::commands::GetGameState>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    Q->command_data().mutable_result()->mutable_state()->CopyFrom(
        get_storage(Q->I(), s)->game_state());
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
                  return v.pointer_self() == k && !v.in_limbo();
                }) != O.end()) {
              auto c1 = args->coordinates();
              auto coords = CoordStruct{.X = c1.x(), .Y = c1.y(), .Z = c1.z()};
              auto cell = MapClass::Instance.get()->TryGetCellAt(coords);
              auto* p = reinterpret_cast<CellClass*>(cell);
              auto b = ra2::abi::ClickMission::call(
                  C->abi(), k, static_cast<Mission>(args->event()),
                  args->target_object(), p, nullptr);
              if (!b) {
                eprintf("click mission error");
              }
            }
          }
        }));
  });
}

// TODO(shmocz): add checks for invalid rtti_id's
auto add_event() {
  return get_cmd<ra2yrproto::commands::AddEvent>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto a = Q->args();
    auto cmd = Q->c;
    Q->save_command_result();
    cmd->pending().store(true);

    get_callback<CBExecuteGameLoopCommand>(Q->I())->work.push(
        make_work<decltype(a)>(a, [cmd](CBYR* C, auto* args) {
          const auto frame_delay = args->frame_delay();

          auto frame = Unsorted::CurrentFrame + frame_delay;
          auto house_index = args->spoof()
                                 ? args->event().house_index()
                                 : HouseClass::CurrentPlayer->ArrayIndex;
          // This is how the frame is computed for protocol zero.
          if (frame_delay == 0) {
            const auto& fsr = Game::Network::FrameSendRate;
            frame =
                (((fsr + Unsorted::CurrentFrame - 1 + Game::Network::MaxAhead) /
                  fsr) *
                 fsr);
          }
          // Set the frame to negative value to indicate that house index and
          // frame number should be spoofed
          frame = frame * (args->spoof() ? -1 : 1);

          EventClass E(static_cast<EventType>(args->event().event_type()),
                       false, static_cast<char>(house_index),
                       static_cast<u32>(frame));

          auto ts = C->abi()->timeGetTime();
          if (args->event().has_production()) {
            auto& ev = args->event().production();
            E.Data.Production = {.RTTI_ID = ev.rtti_id(),
                                 .Heap_ID = ev.heap_id(),
                                 .IsNaval = ev.is_naval()};
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
            (void)EventClass::AddEvent(E, ts);
          } else {
            // generic event
            (void)EventClass::AddEvent(E, ts);
          }

          auto* p = reinterpret_cast<ra2yrproto::CommandResult*>(cmd->result());
          ra2yrproto::commands::AddEvent r2;
          p->mutable_result()->UnpackTo(&r2);
          auto* ev = r2.mutable_result()->mutable_event();
          ev->CopyFrom(args->event());
          ev->set_timing(ts);
          p->mutable_result()->PackFrom(r2);
          cmd->pending().store(false);
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
          auto A = ra2::abi::DVCIterator(TechnoTypeClass::Array.get());
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
              if (cell_s.X < 0 || cell_s.Y < 0) {
                continue;
              }
              auto* cs = reinterpret_cast<CellStruct*>(&cell_s);

              auto p_DisplayClass = 0x87F7E8u;
              // FIXME: rename BuildingClass to BuildingTypeClass
              if (C->abi()->DisplayClass_Passes_Proximity_Check(
                      p_DisplayClass, reinterpret_cast<BuildingTypeClass*>(q),
                      house->array_index(), cs) &&
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

void convert_map_data(ra2yrproto::ra2yr::MapDataSoA* dst,
                      ra2yrproto::ra2yr::MapData* src) {
  const auto sz = src->cells().size();

  for (int i = 0U; i < sz; i++) {
    dst->add_land_type(src->cells(i).land_type());
    dst->add_radiation_level(src->cells(i).radiation_level());
    dst->add_height(src->cells(i).height());
    dst->add_level(src->cells(i).level());
    dst->add_overlay_data(src->cells(i).overlay_data());
    dst->add_tiberium_value(src->cells(i).tiberium_value());
    dst->add_shrouded(src->cells(i).shrouded());
    dst->add_passability(src->cells(i).passability());
  }
  dst->set_map_width(src->width());
  dst->set_map_height(src->height());
}

// windows.h idiotism
#undef GetMessage

///
/// Read a protobuf message from storage determined by command argument type.
///
auto read_value() {
  return get_cmd<ra2yrproto::commands::ReadValue>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto a = Q->args();
    auto* D = Q->command_data().mutable_result()->mutable_data();
    // find the first field that's been set
    auto sf = yrclient::find_set_fields(a.data());
    if (sf.empty()) {
      throw std::runtime_error("no field specified");
    }
    auto* fld = sf[0];

    if (fld->name() == "map_data_soa") {
      ra2yrproto::ra2yr::MapDataSoA MS;
      convert_map_data(&MS, get_storage(Q->I(), s)->mutable_map_data());
      D->mutable_map_data_soa()->CopyFrom(MS);
    } else {
      // TODO: put this stuff to protocol.cpp
      // copy the data
      auto* sval = get_storage(Q->I(), s);
      D->GetReflection()->MutableMessage(D, fld)->CopyFrom(
          sval->GetReflection()->GetMessage(*sval, fld));
    }
  });
}

}  // namespace cmd

std::map<std::string, command::Command::handler_t> commands_yr::get_commands() {
  return {
      cmd::click_event(),            //
      cmd::unit_command(),           //
      cmd::create_callbacks(),       //
      cmd::create_hooks(),           //
      cmd::get_game_state(),         //
      cmd::inspect_configuration(),  //
      cmd::mission_clicked(),        //
      cmd::add_event(),              //
      cmd::place_query(),            //
      cmd::send_message(),           //
      cmd::read_value(),             //
  };
}
