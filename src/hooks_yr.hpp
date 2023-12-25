#pragma once

#include "ra2yrproto/commands_yr.pb.h"
#include "ra2yrproto/ra2yr.pb.h"

#include "async_queue.hpp"
#include "command/is_command.hpp"
#include "instrumentation_service.hpp"
#include "ra2/state_context.hpp"
#include "types.h"

#include <google/protobuf/repeated_ptr_field.h>

#include <chrono>
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

struct CBGameCommand final : public MyCB<CBGameCommand> {
  static constexpr char key_name[] = "cb_game_command";
  static constexpr char key_target[] = "on_frame_update";
  using work_t = std::function<void()>;

  async_queue::AsyncQueue<work_t> work;

  CBGameCommand() = default;

  void put_work(work_t fn) { work.push(fn); }

  void exec() override {
    auto items = work.pop(0, 0.0s);
    for (const auto& it : items) {
      it();
    }
  }
};

template <typename T>
void get_gameloop_command(ra2yrcpp::command::ISCommand<T>* Q,
                          std::function<void(CBGameCommand*)> fn) {
  auto* cb = ra2yrcpp::hooks_yr::CBGameCommand::get(Q->I());
  auto* cmd = Q->c;
  cmd->set_async_handler([cb, fn](auto*) { fn(cb); });
  cb->put_work([cmd]() { cmd->run_async_handler(); });
}

struct YRHook {
  u32 address;
  u32 size;
  const char* name;
};

std::vector<YRHook> get_hooks();

};  // namespace ra2yrcpp::hooks_yr
