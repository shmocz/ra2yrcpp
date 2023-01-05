#include "commands_yr.hpp"

using namespace std::chrono_literals;
using util_command::get_cmd;
using GameT = ra2::game_state::GameState;
using google::protobuf::RepeatedPtrField;
using cb_map_t = std::map<std::string, std::unique_ptr<yrclient::ISCallback>>;

// TODO(shmocz): smarter way to define these
// static keys for callbacks, hooks and misc. data
constexpr char key_callbacks_yr[] = "callbacks_yr";
constexpr char key_game_state[] = "game_state";
constexpr char key_raw_game_state[] = "raw_game_state";
constexpr char key_on_load_game[] = "on_load_game";

template <typename T>
T* ensure_storage_value(yrclient::InstrumentationService* I,
                        yrclient::storage_t* s, const std::string key) {
  if (s->find(key) == s->end()) {
    I->store_value(key, utility::make_uptr<T>());
  }
  return static_cast<T*>(s->at(key).get());
}

static GameT* ensure_raw_gamestate(yrclient::InstrumentationService* I,
                                   yrclient::storage_t* s) {
  return ensure_storage_value<GameT>(
      I, s, static_cast<const char*>(key_raw_game_state));
}

// Utilities to convert raw in-memory game structures to protobuf messages.
namespace protobuf_conv {

static void get_factories(GameT* G,
                          RepeatedPtrField<ra2yrproto::ra2yr::Factory>* res) {
  for (auto& v : G->factory_classes()) {
    auto* f = res->Add();
    f->set_object_id(utility::asint(v->object));
    f->set_owner(utility::asint(v->owner));
    f->set_progress_timer(v->production.value);
  }
}

static void get_houses(GameT* G,
                       RepeatedPtrField<ra2yrproto::ra2yr::House>* res) {
  for (auto& v : G->house_classes()) {
    if (!(v->start_credits > 0)) {
      continue;
    }
    auto* h = res->Add();
    h->set_array_index(v->array_index);
    h->set_current_player(v->current_player);
    h->set_defeated(v->defeated);
    h->set_money(v->money);
    h->set_start_credits(v->start_credits);
    h->set_name(v->name);
    h->set_self(v->self);
    h->set_defeated(v->defeated);
    h->set_is_game_over(v->is_game_over);
    h->set_is_loser(v->is_loser);
    h->set_is_winner(v->is_winner);
    h->set_power_output(v->power_output);
    h->set_power_drain(v->power_drain);
  }
}

static void get_object(ra2yrproto::ra2yr::Object* u,
                       ra2::abstract_types::AbstractTypeClass* atc,
                       ra2::objects::ObjectClass* v) {
  using ra2::general::AbstractType;
  auto* tc = static_cast<ra2::objects::TechnoClass*>(v);
  if (v->id == 0U) {
    eprintf("object has NULL id");
  }
  u->set_pointer_self(v->id);
  u->set_pointer_technotypeclass(
      static_cast<ra2::type_classes::TechnoTypeClass*>(atc)->pointer_self);
  u->set_health(v->health);
  u->set_pointer_house(utility::asint(tc->owner));
  u->set_pointer_initial_owner(utility::asint(tc->originally_owned_by));

  if (yrclient::band<i32>(v->flags, ra2::general::AbstractFlags::Techno) != 0) {
    u->set_armor_multiplier(
        static_cast<ra2::objects::TechnoClass*>(v)->armor_multiplier);
  }

  auto at = ra2::utility::get_AbstractType(utility::asptr(atc->p_vtable));
  // TODO(shmocz): static LUT
  switch (at.t) {
    case AbstractType::BuildingType:
      u->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_BUILDING);
      u->set_owner_country_index(tc->owner_country_index);
      break;
    case AbstractType::InfantryType:
      u->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_INFANTRY);
      break;
    case AbstractType::UnitType:
      u->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_VEHICLE);
      break;
    case AbstractType::AircraftType:
      u->set_object_type(ra2yrproto::ra2yr::ABSTRACT_TYPE_AIRCRAFT);
      break;
    default:
      eprintf("no match for type {}", at.name);
      break;
  }

  if (yrclient::band<i32>(v->flags, ra2::general::AbstractFlags::Foot) != 0) {
    const auto* fc = static_cast<ra2::objects::FootClass*>(tc);
    u->set_speed_multiplier(fc->speed_multiplier);
    u->set_speed_percentage(fc->speed_percentage);
  }
  auto* coords = u->mutable_coordinates();
  coords->set_x(tc->coords.x);
  coords->set_y(tc->coords.y);
  coords->set_z(tc->coords.z);
}

static void get_objects(GameT* G,
                        RepeatedPtrField<ra2yrproto::ra2yr::Object>* res) {
  for (const auto& [k, v] : G->objects) {
    auto* tc = static_cast<ra2::objects::TechnoClass*>(v.get());
    try {
      auto* atc =
          G->abstract_type_classes().at(utility::asint(tc->p_type)).get();
      auto* u = res->Add();  // FIXME: potential memory leak
      protobuf_conv::get_object(u, atc, v.get());
    } catch (const std::exception& e) {
      eprintf("tc={}, p_type_class={}, what={}", static_cast<void*>(tc),
              static_cast<void*>(tc->p_type), e.what());
      continue;
    }
  }
}

static void get_object_type_class(ra2yrproto::ra2yr::ObjectTypeClass* t,
                                  const ra2::objects::AbstractTypeClass* v) {
  t->set_name(v->name);
  if (ra2::utility::is_technotypeclass(utility::asptr(v->p_vtable))) {
    auto* ttc = static_cast<const ra2::type_classes::TechnoTypeClass*>(v);
    t->set_pointer_self(ttc->pointer_self);
    t->set_cost(ttc->cost);
    t->set_soylent(ttc->soylent);
    t->set_armor_type(static_cast<ra2yrproto::ra2yr::Armor>(ttc->armor));
    t->set_pointer_shp_struct(utility::asint(ttc->p_cameo));
  }
}

static void get_object_type_classes(
    GameT* G, RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>* r) {
  for (auto& [k, v] : G->abstract_type_classes()) {
    auto* tc = r->Add();
    protobuf_conv::get_object_type_class(tc, v.get());
  }
}

static void raw_state_to_protobuf(GameT* raw_state,
                                  ra2yrproto::ra2yr::GameState* state,
                                  const bool type_classes = false) {
  if (type_classes) {
    protobuf_conv::get_object_type_classes(raw_state,
                                           state->mutable_object_types());
  }
  protobuf_conv::get_factories(raw_state, state->mutable_factories());
  protobuf_conv::get_houses(raw_state, state->mutable_houses());
  protobuf_conv::get_objects(raw_state, state->mutable_objects());
}

}  // namespace protobuf_conv

struct CBYR : public yrclient::ISCallback {
  yrclient::storage_t* storage{nullptr};
  GameT* raw_game_state_{nullptr};

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
    raw_game_state_ = nullptr;
  }

  template <typename T>
  auto* storage_value(const std::string key) {
    return ensure_storage_value<T>(this->I, this->storage, key);
  }

  GameT* raw_game_state() {
    if (raw_game_state_ == nullptr) {
      raw_game_state_ = storage_value<GameT>(key_raw_game_state);
    }
    return raw_game_state_;
  }

  auto* abi() { return storage_value<ra2::abi::ABIGameMD>("abi"); }

  virtual void exec() { throw std::runtime_error("Not implemented"); }
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
    auto* local_state = storage_value<ra2yrproto::ra2yr::GameState>(key_state);
    if (local_state->load_progresses().empty()) {
      for (auto i = 0U; i < ra2::game_state::MAX_PLAYERS; i++) {
        local_state->add_load_progresses(0.0);
      }
    }
    for (int i = 0; i < local_state->load_progresses().size(); i++) {
      local_state->set_load_progresses(
          i, serialize::read_obj<double>(cpu_state->esi + 0x8 +
                                         i * sizeof(double)));
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
  const std::string record_path;

  std::unique_ptr<yrclient::CompressedOutputStream> out;
  utility::worker_util<ra2yrproto::ra2yr::GameState> work;

  explicit CBSaveState(const std::string&& record_path)
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

  ra2yrproto::ra2yr::GameState state_to_protobuf(
      const bool do_type_classes = false) {
    using namespace ra2::game_state;
    auto* gbuf = storage_value<ra2yrproto::ra2yr::GameState>(key_game_state);
    gbuf->Clear();

    // put load stages
    gbuf->mutable_load_progresses()->CopyFrom(
        storage_value<ra2yrproto::ra2yr::GameState>(
            CBUpdateLoadProgress::key_state)
            ->load_progresses());

    // Parse type classes only once
    if (do_type_classes) {
      ra2::state_parser::parse_AbstractTypeClasses(raw_game_state(),
                                                   p_DVC_AbstractTypeClasses);
      ra2::state_parser::parse_cameos(raw_game_state());
    }

    // Save raw objects
    ra2::state_parser::parse_DVC_HouseClasses(raw_game_state(),
                                              p_DVC_HouseClasses);
    ra2::state_parser::parse_DVC_Objects(raw_game_state(), p_DVC_TechnoClasses);
    ra2::state_parser::parse_DVC_FactoryClasses(raw_game_state(),
                                                p_DVC_FactoryClasses);

    // At this point we're free to leave the callback
    gbuf->set_stage(ra2yrproto::ra2yr::LoadStage::STAGE_INGAME);
    gbuf->set_current_frame(
        serialize::read_obj_le<u32>(ra2::game_state::current_frame));

    protobuf_conv::raw_state_to_protobuf(raw_game_state(), gbuf,
                                         do_type_classes);
    return {*gbuf};
  }

  void exec() override {
    try {
      auto st =
          state_to_protobuf(raw_game_state()->abstract_type_classes().empty());
      work.push(st);
    } catch (const std::exception& e) {
      eprintf("fatal {}", e.what());
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

static void init_callbacks(yrclient::InstrumentationService* I) {
  I->store_value(key_callbacks_yr, utility::make_uptr<cb_map_t>());

  auto t = std::to_string(static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count()));

  auto f = [I](std::unique_ptr<yrclient::ISCallback> c) {
    get_callbacks(I)->try_emplace(c->name(), std::move(c));
  };

  // TODO(shmocz): customizable output path
  const std::string traffic_out = fmt::format("traffic.{}.pb.gz", t);
  std::shared_ptr<yrclient::CompressedOutputStream> out =
      std::make_shared<yrclient::CompressedOutputStream>(traffic_out);

  f(std::make_unique<CBTunnelRecvFrom>(out));
  f(std::make_unique<CBTunnelSendTo>(out));
  f(std::make_unique<CBSaveState>(fmt::format("record.{}.pb.gz", t)));
  f(std::make_unique<CBExitGameLoop>());
  f(std::make_unique<CBExecuteGameLoopCommand>());
  f(std::make_unique<CBUpdateLoadProgress>());
}

static inline void unit_action(const u32 p_object,
                               const ra2yrproto::commands::UnitAction a,
                               const ra2::abi::ABIGameMD* abi) {
  using ra2yrproto::commands::UnitAction;
  switch (a) {
    case UnitAction::ACTION_DEPLOY:
      abi->DeployObject(p_object);  // NB. doesn't work online
      break;
    case UnitAction::ACTION_SELL:
      abi->SellBuilding(p_object);
      break;
    case UnitAction::ACTION_SELECT:
      (void)abi->SelectObject(p_object);
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
          auto* objects = &C->raw_game_state()->objects;
          for (auto k : args->object_addresses()) {
            if (objects->find(k) != objects->end()) {
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
          auto* objects = &C->raw_game_state()->objects;
          for (auto k : args->object_addresses()) {
            if (objects->find(k) != objects->end()) {
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
      h->second.add_callback(
          [cb](hook::Hook* h, void* user_data, X86Regs* state) {
            cb->call(h, user_data, state);
          },
          Q->I(), k, 0U);
    }
  });
}

constexpr std::array<std::pair<const char*, u32>, 6> gg_hooks = {{
    {CBExecuteGameLoopCommand::key_target, 0x55de7f},  //
    {CBExitGameLoop::key_target, 0x72dfb0},            //
    {key_on_load_game, 0x686730},                      //
    {CBTunnelSendTo::key_target, 0x7b3d6f},            //
    {CBTunnelRecvFrom::key_target, 0x7b3f15},          //
    {CBUpdateLoadProgress::key_target, 0x643c62}       //
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
    auto* state = ensure_storage_value<ra2yrproto::ra2yr::GameState>(
        Q->I(), s, key_game_state);
    Q->command_data().mutable_result()->mutable_state()->CopyFrom(*state);
  });
}

auto get_type_classes() {
  return get_cmd<ra2yrproto::commands::GetTypeClasses>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto res = Q->command_data().mutable_result();
    try {
      protobuf_conv::get_object_type_classes(ensure_raw_gamestate(Q->I(), s),
                                             res->mutable_classes());
    } catch (const std::exception& e) {
      eprintf("error! {}", e.what());
      throw;
    }
  });
}

}  // namespace cmd

std::map<std::string, command::Command::handler_t> commands_yr::get_commands() {
  return {cmd::click_event(),       //
          cmd::unit_command(),      //
          cmd::create_callbacks(),  //
          cmd::create_hooks(),      //
          cmd::get_game_state(),    //
          cmd::get_type_classes()};
}
