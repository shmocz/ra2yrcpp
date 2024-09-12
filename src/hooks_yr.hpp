#pragma once

#include "ra2yrproto/commands_yr.pb.h"
#include "ra2yrproto/ra2yr.pb.h"

#include "async_queue.hpp"
#include "command/is_command.hpp"
#include "ra2/abi.hpp"
#include "ra2/state_context.hpp"
#include "types.h"
#include "utility/sync.hpp"

#include <google/protobuf/repeated_ptr_field.h>

#include <cstddef>

#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace util_command {
template <typename T>
struct ISCommand;
}

namespace ra2yrcpp::hooks_yr {

using gpb::RepeatedPtrField;

// General purpose data container to hold resources that need to be freed at
// game exit.
class ServiceData {
 public:
  ServiceData() = default;
  virtual ~ServiceData() = default;
};

enum class ServiceDataId : u32 {
  GAME_COMMAND = 0U,
  RECORD_TRAFFIC = 1U,
  STATE_SAVE = 2U
};

// Should this be a singleton?
struct GameDataYR {
  GameDataYR();

  ra2::abi::ABIGameMD abi;
  ra2yrproto::ra2yr::StorageValue sv;
  ra2yrproto::commands::Configuration cfg;
  std::unique_ptr<ra2::StateContext> ctx{nullptr};
  util::AtomicVariable<bool> game_paused{false};
};

/// Singleton
class MainData {
 public:
  void initialize_service_datas();
  void deinitialize_service_datas();
  GameDataYR* data();
  static MainData& get();
  static util::acquire_t<MainData, std::recursive_mutex> acquire();
  static void lock();
  static void unlock();
  std::map<ServiceDataId, std::unique_ptr<ServiceData>>& service_datas();
  void update_configuration(const ra2yrproto::commands::Configuration& C);

 private:
  static MainData* instance_;
  static std::recursive_mutex lock_;
  std::unique_ptr<GameDataYR> data_;
  std::map<ServiceDataId, std::unique_ptr<ServiceData>> service_datas_;
  MainData();
  ~MainData();
};

class GameDataInterface : public ServiceData {
 public:
  using tc_t = RepeatedPtrField<ra2yrproto::ra2yr::ObjectTypeClass>;
  ra2::abi::ABIGameMD* abi();
  ra2yrproto::commands::Configuration* configuration();
  ra2yrproto::ra2yr::GameState* game_state();
  ra2yrproto::ra2yr::PrerequisiteGroups* prerequisite_groups();
  tc_t* type_classes();
  ra2::StateContext* get_state_context();
  GameDataYR* data();
  /// Update the underlying configuration. Record/traffic paths are determined
  /// at initialization and will be ignored.

 private:
  // NB. cyclic dependency
  GameDataYR* data_{nullptr};
};

class GameCommandData : public GameDataInterface {
 public:
  using work_t = std::function<void()>;
  static constexpr auto id = ServiceDataId::GAME_COMMAND;

  GameCommandData();
  void put_work(work_t fn);
  void consume_work();

  // Get global instance
  static GameCommandData* get();
  static std::unique_ptr<GameCommandData> create();

 private:
  async_queue::AsyncQueue<work_t> work;
};

template <typename T>
void get_gameloop_command(const ra2yrcpp::command::ISCommand<T>* Q,
                          std::function<void(GameCommandData*)> fn) {
  auto* ctx = GameCommandData::get();
  auto* cmd = Q->c;
  cmd->set_async_handler([ctx, fn](auto*) { fn(ctx); });
  ctx->put_work([cmd]() { cmd->run_async_handler(); });
}

void create_all_hooks();
void create_all_hooks(char* hooks_section, std::size_t section_size,
                      void* dll_handle);

};  // namespace ra2yrcpp::hooks_yr
