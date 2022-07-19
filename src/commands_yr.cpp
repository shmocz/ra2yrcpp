#include "commands_yr.hpp"

using namespace commands_yr;
using util_command::ISCommand;
using yrclient::as;
using yrclient::asptr;
using yrclient::parse_address;
using GameT = ra2::game_state::GameState;

static GameT* ensure_raw_gamestate(yrclient::InstrumentationService* I,
                                   yrclient::storage_t* s) {
  const std::string k = "raw_game_state";
  if (s->find(k) == s->end()) {
    I->store_value(k, new GameT(), [](void* data) { delete as<GameT*>(data); });
  }
  return asptr<GameT>(s->at(k).get());
}

static void parse_houses(GameT* G) {
  ra2::state_parser::parse_DVC_HouseClasses(
      G, reinterpret_cast<void*>(ra2::game_state::p_DVC_HouseClasses));
}

static void save_objects(GameT* G) {
  using namespace ra2::state_parser;
  using namespace ra2::abstract_types;
  parse_DVC_Objects(
      G, reinterpret_cast<void*>(ra2::game_state::p_DVC_TechnoClasses));
}

static void parse_building(yrclient::ra2yr::Unit* u,
                           ra2::objects::BuildingClass* b) {
  u->set_unit_type("building");
  u->set_building_state(
      (yrclient::ra2yr::BuildingState)(((u32)b->build_state_type) + 1u));
  u->set_queue_building_state(
      (yrclient::ra2yr::BuildingState)(((u32)b->queue_build_state) + 1u));
}

static void get_unit(yrclient::ra2yr::Unit* u,
                     ra2::abstract_types::AbstractTypeClass* atc,
                     ra2::objects::ObjectClass* v) {
  using ra2::general::AbstractType;
  auto* tc = (ra2::objects::TechnoClass*)(v);
  const auto* ttc = (ra2::type_classes::TechnoTypeClass*)atc;
  if (v->id == 0u) {
    fmt::print("[ERROR] object has NULL id\n");
  }
  u->set_id(v->id);
  u->set_health(v->health);
  u->set_name(atc->name);
  u->set_cost(ttc->cost);
  u->set_soylent(ttc->soylent);
  u->set_owner_id(reinterpret_cast<u32>(tc->owner));
  u->set_armor((yrclient::ra2yr::Armor)ttc->armor);
  u->set_speed(ttc->speed);
  u->set_speed_type((yrclient::ra2yr::SpeedType)ttc->speed_type);
  u->set_points(ttc->points);

  auto at =
      ra2::utility::get_AbstractType(reinterpret_cast<void*>(atc->p_vtable));
  switch (at.t) {
    case AbstractType::BuildingType:
      parse_building(u, (ra2::objects::BuildingClass*)v);
      break;
    case AbstractType::InfantryType:
      u->set_unit_type("infantry");
      break;
    case AbstractType::UnitType:
      u->set_unit_type("vehicle");
      break;
    default:
      fmt::print("NO MATCH {}\n", at.name);
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

using google::protobuf::RepeatedPtrField;

static void get_factories(GameT* G,
                          RepeatedPtrField<yrclient::ra2yr::Factory>* res) {
  for (auto& v : G->factory_classes()) {
    auto* f = res->Add();
    f->set_object_id(reinterpret_cast<u32>(v->object));
    f->set_owner(reinterpret_cast<u32>(v->owner));
    f->set_progress_timer(v->production.value);
  }
}

static void get_houses(GameT* G,
                       RepeatedPtrField<yrclient::ra2yr::House>* res) {
  for (auto& v : G->house_classes()) {
    auto* h = res->Add();
    h->set_array_index(v->array_index);
    h->set_current_player(v->current_player);
    h->set_defeated(v->defeated);
    h->set_money(v->money);
    h->set_start_credits(v->start_credits);
    h->set_name(v->name);
    h->set_self(v->self);
  }
}

static void get_units(GameT* G, RepeatedPtrField<yrclient::ra2yr::Unit>* res) {
  for (const auto& [k, v] : G->objects) {
    auto* tc = (ra2::objects::TechnoClass*)(v.get());
    try {
      auto* atc = G->abstract_type_classes()
                      .at(reinterpret_cast<u32>(tc->p_type))
                      .get();
      auto* u = res->Add();
      get_unit(u, atc, v.get());
    } catch (const std::exception& e) {
      fmt::print(stderr, "[ERROR] atc fail {},{}\n", static_cast<void*>(tc),
                 tc->p_type->p_vtable);
      continue;
    }
  }
}

static void parse_factoryclasses(GameT* G) {
  ra2::state_parser::parse_DVC_FactoryClasses(
      G, reinterpret_cast<void*>(ra2::game_state::p_DVC_FactoryClasses));
}

void cb_save_state(hook::Hook* h, void* data, X86Regs* state) {
  using namespace std::literals::chrono_literals;
  (void)h;
  (void)state;
#ifndef NDEBUG
  static auto cur_time = util::current_time();
#endif
  auto I = static_cast<yrclient::InstrumentationService*>(data);
  auto [mut, s] = I->aq_storage();
  const std::string k = "game_state";
  if (s->find(k) == s->end()) {
    I->store_value(k, new yrclient::ra2yr::GameState(), [](void* data) {
      delete as<yrclient::ra2yr::GameState*>(data);
    });
  }
  auto* r_game_state = ensure_raw_gamestate(I, s);
  // TODO: improve this
  // Save frame directly as protobuf message
  auto game_state = asptr<yrclient::ra2yr::GameState>(I->get_value(k, false));
  game_state->set_current_frame(
      serialize::read_obj_le<u32>(as<u32*>(0xA8ED84)));

#ifndef NDEBUG
  if ((game_state->current_frame() % 60) == 0) {
    auto diff = util::current_time() - cur_time;
    fmt::print(stderr, "frame={},diff={},num units={}\n",
               game_state->current_frame(), diff, r_game_state->objects.size());
    cur_time = util::current_time();
  }
#endif

  // Save raw objects
  parse_houses(r_game_state);
  save_objects(r_game_state);
  parse_factoryclasses(r_game_state);
}

struct hook_entry {
  u32 p_target;
  size_t code_size;
};

struct cb_entry {
  std::string target;
  hook::Hook::hook_cb_t p_callback;
};

static std::map<std::string, hook_entry> hooks = {
    {"on_frame_update", {0xb7e6ac, 8}}};

static std::map<std::string, cb_entry> callbacks = {
    {"save_state", {"on_frame_update", &cb_save_state}}};

static std::map<std::string, command::Command::handler_t> commands = {
    {"CreateHooks",
     [](command::Command* c) {
       ISCommand<yrclient::commands::CreateHooks> Q(c);
       for (auto& [k, v] : hooks) {
         Q.I()->create_hook(k, reinterpret_cast<u8*>(v.p_target), v.code_size);
       }
     }},
    {"CreateCallbacks",
     [](command::Command* c) {
       ISCommand<yrclient::commands::CreateCallbacks> Q(c);
       auto [lk, hhooks] = Q.I()->aq_hooks();
       for (auto& [k, v] : callbacks) {
         auto target = v.target;
         auto h = std::find_if(hhooks->begin(), hhooks->end(), [&](auto& a) {
           return (a.second.name() == target);
         });
         if (h == hhooks->end()) {
           throw yrclient::general_error(std::string("No such hook: ") +
                                         target);
         }
         if (h->second.callbacks().size() > 0) {
           throw yrclient::general_error(
               std::string("hook already has a callback"));
         }

         h->second.add_callback(hook::Hook::HookCallback{v.p_callback, Q.I()});
       }
     }},
    {"GetGameState",
     [](command::Command* c) {
       ISCommand<yrclient::commands::GetGameState> Q(c);
       // Copy saved game state
       auto [mut, s] = Q.I()->aq_storage();
       auto res = Q.command_data().mutable_result();
       auto* val = asptr<yrclient::ra2yr::GameState>(s->at("game_state").get());
       auto* state = res->mutable_state();
       // state->CopyFrom(*val);
       auto* G = ensure_raw_gamestate(Q.I(), s);
       get_units(G, state->mutable_units());
       get_factories(G, state->mutable_factories());
       get_houses(G, state->mutable_houses());
     }},
    {"GetTypeClasses",
     [](command::Command* c) {
       using namespace ra2::state_parser;
       using namespace ra2::abstract_types;
       ISCommand<yrclient::commands::GetTypeClasses> Q(c);
       auto [mut, s] = Q.I()->aq_storage();
       auto G = ensure_raw_gamestate(Q.I(), s);
       auto res = Q.command_data().mutable_result();
       parse_AbstractTypeClasses(
           G,
           reinterpret_cast<void*>(ra2::game_state::p_DVC_AbstractTypeClasses));
       for (auto& [k, v] : G->abstract_type_classes()) {
         auto* tc = res->add_classes();
         tc->set_name(v->name);
       }
     }},
    {"GetObjects",
     [](command::Command* c) {
       using namespace ra2::state_parser;
       using namespace ra2::abstract_types;
       ISCommand<yrclient::commands::GetObjects> Q(c);
       auto [mut, s] = Q.I()->aq_storage();
       auto G = ensure_raw_gamestate(Q.I(), s);
       auto res = Q.command_data().mutable_result();
       get_units(G, res->mutable_units());
     }},
    {"GetHouses", [](command::Command* c) {
       using namespace ra2::state_parser;
       using namespace ra2::abstract_types;
       ISCommand<yrclient::commands::GetHouses> Q(c);
       auto [mut, s] = Q.I()->aq_storage();
       auto G = ensure_raw_gamestate(Q.I(), s);
       auto res = Q.command_data().mutable_result();
       parse_DVC_HouseClasses(
           G, reinterpret_cast<void*>(ra2::game_state::p_DVC_HouseClasses));
       // WIP: typeclass and superweapons
       DPRINTF("num classes=%d\n", G->house_classes().size());
       get_houses(G, res->mutable_houses());
     }}};

std::map<std::string, command::Command::handler_t>*
commands_yr::get_commands() {
  return &commands;
}
