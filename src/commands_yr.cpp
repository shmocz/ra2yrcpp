#include "commands_yr.hpp"

using namespace commands_yr;
using util_command::ISCommand;
using yrclient::as;
using yrclient::asptr;
using yrclient::parse_address;

void cb_save_state(hook::Hook* h, void* data, X86Regs* state) {
  (void)h;
  (void)state;
  auto I = static_cast<yrclient::InstrumentationService*>(data);
  auto [mut, s] = I->aq_storage();
  const std::string k = "game_state";
#if 1
  if (!(s->find(k) != s->end())) {
    I->store_value(k, new yrclient::ra2yr::GameState(), [](void* data) {
      delete as<yrclient::ra2yr::GameState*>(data);
    });
  }
#endif
  auto game_state = asptr<yrclient::ra2yr::GameState>(I->get_value(k));
  game_state->set_current_frame(serialize::read_obj_le<u32>(as<u8*>(0xA8ED84)));
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
    {"on_frame_update", {0x55de7f, 8}}};

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
       auto val = asptr<yrclient::ra2yr::GameState>(s->at("game_state").get());
       res->mutable_state()->CopyFrom(*val);
     }},
};

std::map<std::string, command::Command::handler_t>*
commands_yr::get_commands() {
  return &commands;
}
