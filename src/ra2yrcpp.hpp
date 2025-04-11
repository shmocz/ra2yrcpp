#pragma once
#include "config.hpp"
#include "instrumentation_service.hpp"

#include <map>
#include <memory>

namespace ra2yrcpp {

// Global ra2yrcpp instance
struct Main {
  ra2yrcpp::InstrumentationService* service_;
  std::map<std::uintptr_t, hook::Hook> hooks_;
  std::unique_ptr<ra2yrcpp::config::Config> cfg_;
  void create_all_hooks();
  void create_all_hooks(char* hooks_section, std::size_t section_size,
                        void* dll_handle);
  void create_hook(hook::HookEntry h, hook::hook_fn f);
  void load_configuration(const char* config_path);
  void start_service();
  ra2yrcpp::config::Config& config();

  // Get global instance
  static Main* get();
};

}  // namespace ra2yrcpp
