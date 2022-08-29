#include "commands_yr.hpp"

using namespace commands_yr;
using namespace std::chrono_literals;
using util_command::ISCommand;
using yrclient::as;
using yrclient::asptr;
using GameT = ra2::game_state::GameState;
using google::protobuf::RepeatedPtrField;
using cb_map_t = std::map<std::string, std::unique_ptr<yrclient::ISCallback>>;

// TODO: smarter way to define these
constexpr char key_callbacks_yr[] = "callbacks_yr";
constexpr char key_game_state[] = "game_state";
constexpr char key_raw_game_state[] = "raw_game_state";
constexpr char key_state_rb[] = "state_rb";
constexpr char key_exit_gameloop[] = "cb_exit_gameloop";
constexpr char key_gameloop[] = "cb_gameloop";
constexpr char key_on_gameloop_exit[] = "on_gameloop_exit";
constexpr char key_on_load_game[] = "on_load_game";
constexpr char key_on_frame_update[] = "on_frame_update";
constexpr char key_load_game[] = "load_game";
constexpr char key_save_state[] = "save_state";
constexpr char key_execute_gameloop_command[] = "cb_execute_gameloop_command";

struct hook_entry {
  u32 p_target;
  size_t code_size;
};

struct cb_entry {
  std::string target;
  std::unique_ptr<yrclient::ISCallback> p_callback;
};

static std::map<std::string, hook_entry> g_hooks = {
    {key_on_frame_update, {0xb7e6ac, 8}},
    {key_on_gameloop_exit, {0xb7e6bc, 6}},
    {key_on_load_game, {0xb7e6dc, 11}},
};

template <typename T>
T* ensure_storage_value(yrclient::InstrumentationService* I,
                        yrclient::storage_t* s, const std::string key) {
  if (s->find(key) == s->end()) {
    I->store_value(key, new T(), [](void* data) { delete as<T*>(data); });
  }
  return as<T*>(s->at(key).get());
}

static GameT* ensure_raw_gamestate(yrclient::InstrumentationService* I,
                                   yrclient::storage_t* s) {
  return ensure_storage_value<GameT>(I, s, key_raw_game_state);
}

static void get_factories(GameT* G,
                          RepeatedPtrField<yrclient::ra2yr::Factory>* res) {
  for (auto& v : G->factory_classes()) {
    auto* f = res->Add();
    f->set_object_id(reinterpret_cast<u32>(v->object));
    f->set_owner(reinterpret_cast<u32>(v->owner));
    f->set_progress_timer(v->production.value);
  }
}

static bool is_valid_house(const ra2::objects::HouseClass& H) {
  return H.start_credits > 0;
}

static void get_houses(GameT* G,
                       RepeatedPtrField<yrclient::ra2yr::House>* res) {
  for (auto& v : G->house_classes()) {
    if (!is_valid_house(*v)) {
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

static void get_object(yrclient::ra2yr::Object* u,
                       ra2::abstract_types::AbstractTypeClass* atc,
                       ra2::objects::ObjectClass* v) {
  using ra2::general::AbstractType;
  auto* tc = (ra2::objects::TechnoClass*)(v);
  const auto* ttc = (ra2::type_classes::TechnoTypeClass*)atc;
  if (v->id == 0u) {
    eprintf("object has NULL id");
  }
  u->set_pointer_self(v->id);
  u->set_pointer_technotypeclass(ttc->pointer_self);
  u->set_health(v->health);
  u->set_pointer_house(reinterpret_cast<u32>(tc->owner));
  u->set_pointer_initial_owner(reinterpret_cast<u32>(tc->originally_owned_by));

  if (yrclient::band<i32>(v->flags, ra2::general::AbstractFlags::Techno)) {
    auto* t = (ra2::objects::TechnoClass*)v;
    u->set_armor_multiplier(t->armor_multiplier);
  }

  auto at =
      ra2::utility::get_AbstractType(reinterpret_cast<void*>(atc->p_vtable));
  // TODO: static LUT
  switch (at.t) {
    case AbstractType::BuildingType:
      u->set_object_type(yrclient::ra2yr::ABSTRACT_TYPE_BUILDING);
      u->set_owner_country_index(tc->owner_country_index);
      break;
    case AbstractType::InfantryType:
      u->set_object_type(yrclient::ra2yr::ABSTRACT_TYPE_INFANTRY);
      break;
    case AbstractType::UnitType:
      u->set_object_type(yrclient::ra2yr::ABSTRACT_TYPE_VEHICLE);
      break;
    case AbstractType::AircraftType:
      u->set_object_type(yrclient::ra2yr::ABSTRACT_TYPE_AIRCRAFT);
      break;
    default:
      eprintf("no match for type {}", at.name);
      break;
  }

  if (yrclient::band<i32>(v->flags, ra2::general::AbstractFlags::Foot)) {
    const auto* fc = (ra2::objects::FootClass*)tc;
    u->set_speed_multiplier(fc->speed_multiplier);
    u->set_speed_percentage(fc->speed_percentage);
  }
  auto* coords = u->mutable_coordinates();
  coords->set_x(tc->coords.x);
  coords->set_y(tc->coords.y);
  coords->set_z(tc->coords.z);
}

static void get_object_type_class(yrclient::ra2yr::ObjectTypeClass* t,
                                  ra2::objects::AbstractTypeClass* v) {
  t->set_name(v->name);
  if (ra2::utility::is_technotypeclass(reinterpret_cast<void*>(v->p_vtable))) {
    auto* ttc = (ra2::type_classes::TechnoTypeClass*)v;
    t->set_pointer_self(ttc->pointer_self);
    t->set_cost(ttc->cost);
    t->set_soylent(ttc->soylent);
    t->set_armor_type((yrclient::ra2yr::Armor)ttc->armor);
    t->set_pointer_shp_struct(reinterpret_cast<u32>(ttc->p_cameo));
  }
}

static void get_object_type_classes(
    GameT* G, RepeatedPtrField<yrclient::ra2yr::ObjectTypeClass>* r) {
  for (const auto& [k, v] : G->abstract_type_classes()) {
    auto* t = r->Add();
    get_object_type_class(t, v.get());
  }
}

static void get_objects(GameT* G,
                        RepeatedPtrField<yrclient::ra2yr::Object>* res) {
  for (const auto& [k, v] : G->objects) {
    auto* tc = (ra2::objects::TechnoClass*)(v.get());
    try {
      auto* atc = G->abstract_type_classes()
                      .at(reinterpret_cast<u32>(tc->p_type))
                      .get();
      auto* u = res->Add();
      get_object(u, atc, v.get());
    } catch (const std::exception& e) {
      eprintf("tc={}, p_type_class={}, what={}", static_cast<void*>(tc),
              static_cast<void*>(tc->p_type), e.what());
      continue;
    }
  }
}

static void raw_state_to_protobuf(GameT* raw_state,
                                  yrclient::ra2yr::GameState* state,
                                  const bool type_classes = false) {
  state->set_current_frame(
      serialize::read_obj_le<u32>(as<u32*>(ra2::game_state::current_frame)));

  if (type_classes) {
    get_object_type_classes(raw_state, state->mutable_object_types());
  }
  get_factories(raw_state, state->mutable_factories());
  get_houses(raw_state, state->mutable_houses());
  get_objects(raw_state, state->mutable_objects());
}

struct CBYR : public yrclient::ISCallback {
  yrclient::storage_t* storage;
  GameT* raw_game_state_;

  CBYR() : storage(nullptr), raw_game_state_(nullptr) {}

  void do_call(yrclient::InstrumentationService* I) override {
    auto [mut, s] = I->aq_storage();
    storage = s;
    main();
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

  virtual void main() { throw std::runtime_error("Not implemented"); }
};

struct CBExitGameLoop : public yrclient::ISCallback {
  std::string name() override { return key_exit_gameloop; }
  std::string target() override { return key_on_gameloop_exit; }
  CBExitGameLoop() {}
  ~CBExitGameLoop() override {}

  void do_call(yrclient::InstrumentationService* I) override {
    // Delete all callbacks except ourselves
    // NB. the corresponding HookCallback must be removed from Hook object
    // (shared_ptr would be handy here)

    auto [lk, hhooks] = I->aq_hooks();
    auto* callbacks = asptr<cb_map_t>(I->get_value(key_callbacks_yr, true));
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

struct CBBeginLoad : public CBYR {
  CBBeginLoad() {}

  std::string name() override { return key_load_game; }

  std::string target() override { return key_on_load_game; }

  void main() override {
    auto* state = storage_value<yrclient::ra2yr::GameState>(key_game_state);
    state->set_stage(yrclient::ra2yr::LoadStage::STAGE_LOADING);
  }
};

struct CBExecuteGameLoopCommand : public CBYR {
  async_queue::AsyncQueue<std::function<void(CBYR*)>> work;

  CBExecuteGameLoopCommand() {}

  std::string name() override { return key_execute_gameloop_command; }

  std::string target() override { return key_on_frame_update; }

  void main() override {
    auto items = work.pop(0, 0ms);
    for (auto& it : items) {
      it(this);
    }
  }
};

struct CBSaveState : public CBYR {
  const std::string record_path;
  yrclient::CompressedOutputStream* out;
  std::thread worker_thread;
  async_queue::AsyncQueue<yrclient::ra2yr::GameState*> work;

  explicit CBSaveState(const std::string record_path)
      : record_path(record_path),
        out(new yrclient::CompressedOutputStream(record_path)),
        worker_thread([&]() { this->worker(); }) {}

  ~CBSaveState() override {
    // NB. exit worker before closing the handle.
    work.push(nullptr);
    worker_thread.join();
    delete out;
  }

  void serialize_state(yrclient::ra2yr::GameState* G) {
    if (out != nullptr) {
      google::protobuf::io::CodedOutputStream co(&out->s_g);

      if (!yrclient::write_message(G, &co)) {
        throw std::runtime_error("write_message");
      }
    }
  }

  void worker() {
    while (true) {
      auto V = work.pop(1, 1000ms * (3600));
      auto w = V.back();
      if (w == nullptr) {
        break;
      }
      serialize_state(w);
      delete w;
    }
  }

  std::string name() override { return key_save_state; }

  std::string target() override { return key_on_frame_update; }

  bool has_typeclasses() {
    return !this->raw_game_state()->abstract_type_classes().empty();
  }

  yrclient::ra2yr::GameState* state_to_protobuf(
      const bool do_type_classes = false) {
    auto* gbuf = storage_value<yrclient::ra2yr::GameState>(key_game_state);
    gbuf->Clear();

    // Parse type classes only once
    if (do_type_classes) {
      ra2::state_parser::parse_AbstractTypeClasses(
          raw_game_state(),
          reinterpret_cast<void*>(ra2::game_state::p_DVC_AbstractTypeClasses));
      ra2::state_parser::parse_cameos(raw_game_state());
    }

    // Save raw objects
    ra2::state_parser::parse_DVC_HouseClasses(
        raw_game_state(),
        reinterpret_cast<void*>(ra2::game_state::p_DVC_HouseClasses));
    ra2::state_parser::parse_DVC_Objects(
        raw_game_state(),
        reinterpret_cast<void*>(ra2::game_state::p_DVC_TechnoClasses));
    ra2::state_parser::parse_DVC_FactoryClasses(
        raw_game_state(),
        reinterpret_cast<void*>(ra2::game_state::p_DVC_FactoryClasses));

    // At this point we're free to leave the callback
    gbuf->set_stage(yrclient::ra2yr::LoadStage::STAGE_INGAME);
    raw_state_to_protobuf(raw_game_state(), gbuf, do_type_classes);
    auto* gnew = new yrclient::ra2yr::GameState();
    gnew->CopyFrom(*gbuf);
    return gnew;
  }

  void main() override { work.push(state_to_protobuf(!has_typeclasses())); }
};

// TODO: customizable output path
static void init_callbacks(yrclient::InstrumentationService* I) {
  I->store_value(key_callbacks_yr, new cb_map_t(),
                 [](void* data) { delete as<cb_map_t*>(data); });

  auto t = std::to_string(static_cast<std::uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count()));

  std::string record_out = yrclient::join_string({"record", t, "pb.gz"}, ".");
  auto* v = asptr<cb_map_t>(I->get_value(key_callbacks_yr, false));
  std::vector<yrclient::ISCallback*> cbs{
      new CBBeginLoad(), new CBSaveState(record_out), new CBExitGameLoop(),
      new CBExecuteGameLoopCommand()};
  for (auto* cb : cbs) {
    v->try_emplace(cb->name(), cb);
  }
}

void unit_action(const u32 p_object, const yrclient::commands::UnitAction a,
                 const ra2::abi::ABIGameMD* abi) {
  using namespace yrclient::commands;
  switch (a) {
    case UnitAction::ACTION_DEPLOY:
      abi->DeployObject(p_object);
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

static std::map<std::string, command::Command::handler_t> commands = {
    {"CreateHooks",
     [](command::Command* c) {
       ISCommand<yrclient::commands::CreateHooks> Q(c);
       for (auto& [k, v] : g_hooks) {
         Q.I()->create_hook(k, reinterpret_cast<u8*>(v.p_target), v.code_size);
       }
     }},
    {"CreateCallbacks",
     [](command::Command* c) {
       ISCommand<yrclient::commands::CreateCallbacks> Q(c);
       auto [lk_s, s] = Q.I()->aq_storage();
       // Create ABI
       (void)ensure_storage_value<ra2::abi::ABIGameMD>(Q.I(), s, "abi");

       if (s->find(key_callbacks_yr) == s->end()) {
         init_callbacks(Q.I());
       }
       auto g_callbacks = asptr<cb_map_t>(s->at(key_callbacks_yr).get());
       lk_s.unlock();
       auto [lk, hhooks] = Q.I()->aq_hooks();
       for (auto& [k, v] : *g_callbacks) {
         auto target = v->target();
         auto cb = v.get();
         auto h = std::find_if(hhooks->begin(), hhooks->end(), [&](auto& a) {
           return (a.second.name() == target);
         });
         if (h == hhooks->end()) {
           throw yrclient::general_error(
               fmt::format("No such hook {}", target));
         }

         const std::string hook_name = k;
         auto& tmp_cbs = h->second.callbacks();
         if (std::find_if(tmp_cbs.begin(), tmp_cbs.end(),
                          [&hook_name](auto& a) {
                            return a.name == hook_name;
                          }) != tmp_cbs.end()) {
           throw yrclient::general_error(fmt::format(
               "Hook {} already has a callback {}", target, hook_name));
         }

         dprintf("add hook, target={} cb={}", target, hook_name);
         h->second.add_callback(hook::Hook::HookCallback{
             [cb](hook::Hook* h, void* user_data, X86Regs* state) {
               cb->call(h, user_data, state);
             },
             Q.I(), 0u, 0u, k});
       }
     }},
    {"GetGameState",
     [](command::Command* c) {
       ISCommand<yrclient::commands::GetGameState> Q(c);
       // Copy saved game state
       auto [mut, s] = Q.I()->aq_storage();
       auto res = Q.command_data().mutable_result();
       auto* state = ensure_storage_value<yrclient::ra2yr::GameState>(
           Q.I(), s, key_game_state);
       res->mutable_state()->CopyFrom(*state);
     }},
    {"GetTypeClasses",
     [](command::Command* c) {
       ISCommand<yrclient::commands::GetTypeClasses> Q(c);
       auto [mut, s] = Q.I()->aq_storage();
       auto G = ensure_raw_gamestate(Q.I(), s);
       auto res = Q.command_data().mutable_result();
       get_object_type_classes(G, res->mutable_classes());
     }},
    {"GetObjects",
     [](command::Command* c) {
       ISCommand<yrclient::commands::GetObjects> Q(c);
       auto [mut, s] = Q.I()->aq_storage();
       auto G = ensure_raw_gamestate(Q.I(), s);
       auto res = Q.command_data().mutable_result();
       yrclient::ra2yr::GameState state;
       raw_state_to_protobuf(G, &state);
       res->mutable_units()->CopyFrom(state.units());
     }},
    {"UnitCommand", [](command::Command* c) {
       ISCommand<yrclient::commands::UnitCommand> Q(c);
       auto [mut, s] = Q.I()->aq_storage();

       auto* v = asptr<cb_map_t>(Q.I()->get_value(key_callbacks_yr, false));
       auto CB = asptr<CBExecuteGameLoopCommand>(
           v->at(key_execute_gameloop_command).get());
       auto a = Q.args();
       const auto action = a.action();
       auto* addrs = new std::vector<uint32_t>();
       addrs->insert(addrs->begin(), a.object_addresses().begin(),
                     a.object_addresses().end());
       CB->work.push([addrs, action](CBYR* C) {
         auto G = C->raw_game_state();
         for (auto k : *addrs) {
           auto it = G->objects.find(reinterpret_cast<std::uint32_t*>(k));
           if (it != G->objects.end()) {
             auto* abi = ensure_storage_value<ra2::abi::ABIGameMD>(
                 C->I, C->storage, "abi");
             unit_action(k, action, abi);
           } else {
             throw yrclient::general_error(fmt::format("not found={}", k));
           }
         }
         delete addrs;
       });
     }}};

std::map<std::string, command::Command::handler_t>*
commands_yr::get_commands() {
  return &commands;
}
