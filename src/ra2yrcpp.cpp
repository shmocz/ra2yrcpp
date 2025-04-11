#include "ra2yrcpp.hpp"

#include "instrumentation_service.hpp"
#include "is_context.hpp"
#include "win32/windows_utils.hpp"

#include <memory>
#include <string>

using namespace ra2yrcpp;

void Main::create_hook(hook::HookEntry h, hook::hook_fn f) {
  iprintf("name={},target={:#x},size_bytes={}", h.name, h.address, h.size);
  if (hooks_.find(h.address) != hooks_.end()) {
    throw std::runtime_error(
        fmt::format("Can't overwrite existing hook (name={} address={})",
                    h.name, reinterpret_cast<void*>(h.address)));
  }
  hooks_.try_emplace(h.address, h, f);
}

void Main::create_all_hooks(char* hooks_section, std::size_t section_size,
                            void* dll_handle) {
  // For each hook entry
  const char* hooks_end = hooks_section + section_size;
  for (char* p = hooks_section; p < hooks_end; p += sizeof(hook::HookEntry)) {
    auto* H = reinterpret_cast<hook::HookEntry*>(p);
    // Get corresponding function
    // std::string fn_name = "_" + std::string(H->hookName);
    std::string fn_name = std::string(H->name);
    auto* proc_address = reinterpret_cast<hook::hook_fn>(
        windows_utils::get_proc_address(fn_name, dll_handle));
    if (proc_address == nullptr) {
      throw std::runtime_error(
          fmt::format("couldn't find hook function: {}", fn_name));
    }
    // Patch target code
    create_hook(*H, proc_address);
  }
}

void Main::create_all_hooks() {
  auto P = process::get_current_process();
  void* dll = windows_utils::find_dll(cfg::DLL_NAME);
  if (dll == nullptr) {
    throw std::runtime_error("ra2yrcpp main DLL not loaded");
  }

  // Get syringe section
  auto section = windows_utils::find_section(dll, ".syhks00");
  if (section.data == nullptr) {
    throw std::runtime_error(".syhks00 section not found from DLL");
  }

  create_all_hooks(reinterpret_cast<char*>(section.data), section.length, dll);
}

Main* Main::get() {
  static Main* I = nullptr;
  if (I == nullptr) {
    I = new Main();
  }
  return I;
}

void Main::load_configuration(const char* config_path) {
  std::string json = "{}";
  if (config_path != nullptr) {
    std::ifstream ifs(config_path);
    json = std::string((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
  }
  cfg_ = std::make_unique<ra2yrcpp::config::Config>(json);

  if (cfg_->c().log_filename.empty()) {
    ra2yrcpp::logging::close_output_handle();
    // TODO: Check errors
    (void)ra2yrcpp::logging::set_output_handle(nullptr);
  }
}

void Main::start_service() {
  if (service_ == nullptr) {
    InstrumentationService::Options o;
    o.server.allowed_hosts_regex = cfg_->c().allowed_hosts_regex;
    o.server.port = cfg_->c().port;
    o.server.max_connections = cfg_->c().max_connections;
    service_ = is_context::make_is(o);
  }
}

ra2yrcpp::config::Config& Main::config() { return *cfg_; }
