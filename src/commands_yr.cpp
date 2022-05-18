#include "commands_yr.hpp"
#include "errors.hpp"
#include "util_string.hpp"
#include "protocol/protocol.hpp"

using namespace commands_yr;
using yrclient::parse_address;

void cb_save_state(hook::Hook* h, void* data, X86Regs* state) {
  (void)h;
  (void)state;
  auto I = static_cast<yrclient::InstrumentationService*>(data);
  u8* p_current_frame = reinterpret_cast<u8*>(0xA8ED84);
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

static std::map<std::string, yrclient::IServiceCommand> commands = {
    {"create_hooks",
     [](yrclient::IServiceArgs args) {
       auto [I, cmd_args, result] = get_args(args);
       for (auto& [k, v] : hooks) {
         I->create_hook(k, reinterpret_cast<u8*>(v.p_target), v.code_size);
       }
       return std::make_unique<vecu8>();
     }},
    {"create_callbacks",
     [](yrclient::IServiceArgs args) {
       auto [I, cmd_args, result] = get_args(args);
       auto [lk, hhooks] = I->aq_hooks();
       for (auto& [k, v] : callbacks) {
         auto target = v.target;
         auto h = std::find_if(hhooks->begin(), hhooks->end(), [&](auto& a) {
           return (a.second.name() == target);
         });
         if (h == hhooks->end()) {
           throw yrclient::general_error(std::string("No such hook: ") +
                                         target);
         }
         h->second.add_callback(hook::Hook::HookCallback{v.p_callback, I});
       }
       return std::make_unique<vecu8>();
     }},
    {"get_state", [](yrclient::IServiceArgs args) {
       auto [I, cmd_args, result] = get_args(args);
       std::string name = cmd_args[0];
       std::string address = cmd_args[1];
       const size_t code_length = std::stoi(cmd_args[2]);
       I->create_hook(name, reinterpret_cast<u8*>(parse_address(address)),
                      code_length);
       return std::make_unique<vecu8>();
     }}};

std::map<std::string, yrclient::IServiceCommand>* commands_yr::get_commands() {
  return &commands;
}
