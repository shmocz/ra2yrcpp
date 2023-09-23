#pragma once

#include "ra2yrproto/commands_yr.pb.h"
#include "ra2yrproto/ra2yr.pb.h"

#include "async_queue.hpp"
#include "command/command_manager.hpp"
#include "command/is_command.hpp"
#include "instrumentation_service.hpp"
#include "logging.hpp"
#include "ra2/state_context.hpp"
#include "types.h"

#include <google/protobuf/repeated_ptr_field.h>

#include <chrono>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace util_command {
template <typename T>
struct ISCommand;
}

namespace ra2 {
namespace abi {
class ABIGameMD;
}
}  // namespace ra2

namespace ra2yrcpp::hooks_yr {

constexpr char key_callbacks_yr[] = "callbacks_yr";
constexpr char key_configuration[] = "yr_config";

namespace {
using namespace std::chrono_literals;
}

using gpb::RepeatedPtrField;
using cb_map_t = std::map<std::string, std::unique_ptr<ra2yrcpp::ISCallback>>;

struct CBYR : public ra2yrcpp::ISCallback {
  using tc_t = RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>;

  ra2yrcpp::storage_t* storage{nullptr};
  ra2::abi::ABIGameMD* abi_{nullptr};
  ra2yrproto::commands::Configuration* config_{nullptr};
  ra2::StateContext* state_context_{nullptr};

  CBYR();
  ra2::abi::ABIGameMD* abi();
  ra2yrproto::commands::Configuration* configuration();
  void do_call() override;
  virtual void exec() = 0;
  ra2yrproto::ra2yr::GameState* game_state();
  auto* prerequisite_groups();
  tc_t* type_classes();
  ra2::StateContext* get_state_context();
};

/// Get all currently active callback objects.
cb_map_t* get_callbacks(ra2yrcpp::InstrumentationService* I,
                        const bool acquire = false);

template <typename D, typename B = CBYR>
struct MyCB : public B {
  std::string name() override { return D::key_name; }

  std::string target() override { return D::key_target; }

  static D* get(ra2yrcpp::InstrumentationService* I) {
    return reinterpret_cast<D*>(get_callbacks(I)->at(D::key_name).get());
  }
};

// TODO(shmocz): reduce calls to this
template <typename T, typename... ArgsT>
T* ensure_storage_value(ra2yrcpp::InstrumentationService* I,
                        const std::string key, ArgsT... args) {
  if (I->storage().find(key) == I->storage().end()) {
    I->store_value<T>(key, args...);
  }
  return static_cast<T*>(I->storage().at(key).get());
}

// TODO(shmocz): try to pick a name to avoid confusion with IService's storage
ra2yrproto::ra2yr::StorageValue* get_storage(
    ra2yrcpp::InstrumentationService* I);

ra2yrproto::commands::Configuration* ensure_configuration(
    ra2yrcpp::InstrumentationService* I);

void init_callbacks(ra2yrcpp::InstrumentationService* I);

struct work_item {
  CBYR* cb;
  command::iservice_cmd* cmd;
  std::function<void(work_item*)> fn;
};

struct CBExecuteGameLoopCommand final : public MyCB<CBExecuteGameLoopCommand> {
  static constexpr char key_name[] = "cb_execute_gameloop_command";
  static constexpr char key_target[] = "on_frame_update";

  async_queue::AsyncQueue<work_item> work;

  CBExecuteGameLoopCommand() = default;

  void put_work(std::function<void(work_item*)> fn, ra2yrcpp::cmd_t* cmd) {
    bool async = cmd->pending().get();
    work.push({this, cmd, [async, fn](auto* it) {
                 try {
                   fn(it);
                 } catch (const std::exception& e) {
                   eprintf("gameloop command: {}", e.what());
                   if (async) {
                     it->cmd->set_error(e.what());
                   }
                 }
                 if (async) {
                   it->cmd->pending().store(false);
                 }
               }});
  }

  void exec() override {
    auto items = work.pop(0, 0.0s);
    for (auto& it : items) {
      it.fn(&it);
    }
  }
};

template <typename T>
void put_gameloop_command(
    ra2yrcpp::command::ISCommand<T>* Q,
    std::function<void(ra2yrcpp ::hooks_yr::work_item*)> fn) {
  CBExecuteGameLoopCommand::get(Q->I())->put_work(fn, Q->c);
}

struct YRHook {
  const char* name;
  u32 size;
};

std::vector<YRHook> get_hooks();

};  // namespace ra2yrcpp::hooks_yr
