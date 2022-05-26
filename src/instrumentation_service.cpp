#include "instrumentation_service.hpp"
#include "commands_builtin.hpp"
#include "commands_yr.hpp"

using namespace yrclient;

void InstrumentationService::add_command_new(
    std::string name, command::Command::handler_t fn,
    command::Command::deleter_t deleter) {
  cmd_manager_new_.factory().add_entry(name, fn, deleter);
}

std::vector<process::thread_id_t>
InstrumentationService::get_connection_threads() {
  std::vector<process::thread_id_t> res;
  auto& C = server().connections();
  std::transform(C.begin(), C.end(), std::back_inserter(res),
                 [](auto& ctx) { return ctx->thread_id; });
  return res;
}

void InstrumentationService::create_hook(std::string name, u8* target,
                                         const size_t code_length) {
  std::unique_lock<std::mutex> lk(mut_hooks_);
  auto tids = get_connection_threads();
  hooks_.try_emplace(target, reinterpret_cast<addr_t>(target), code_length,
                     name, tids);
}

command::CommandManager& InstrumentationService::cmd_manager_new() {
  return cmd_manager_new_;
}

server::Server& InstrumentationService::server() { return server_; }
std::map<u8*, hook::Hook>& InstrumentationService::hooks() { return hooks_; }

template <typename T>
yrclient::Response* reply_body(yrclient::Response* R, const T& body) {
  if (!R->mutable_body()->PackFrom(body)) {
    throw yrclient::general_error("Couldn't pack response body");
  }
  return R;
}

yrclient::Response reply_error(std::string message) {
  yrclient::Response R;
  yrclient::TextResponse E;
  E.mutable_message()->assign(message);
  // E.set_error_message(message);
  R.set_code(RESPONSE_ERROR);

  return *reply_body(&R, E);
}

yrclient::Response reply_ok(std::string message) {
  yrclient::Response R;
  yrclient::TextResponse T;
  T.mutable_message()->assign(message);
  R.set_code(RESPONSE_OK);
  return *reply_body(&R, T);
}

yrclient::Response InstrumentationService::flush_results_new(
    const u64 queue_id) {
  auto results = cmd_manager_new().flush_results(queue_id);
  yrclient::Response R;
  yrclient::NewCommandPollResult P;
  // auto& c_queue = res_queue->at(queue_id);
  while (!results.empty()) {
    auto item = results.back();
    auto* cmd_result = as<google::protobuf::Message*>(item->result());
    auto R = P.add_results();
    R->PackFrom(*cmd_result);
    results.pop_back();
  }

  R.mutable_body()->PackFrom(P);
  return R;
}

yrclient::Response handle_cmd_ng(InstrumentationService* I,
                                 connection::Connection* C, vecu8* bytes,
                                 Command* cmd) {
  (void)bytes;
  using yrclient::split_string;
  // TODO: reduce amount of copies we make
  vecu8 result;
  auto client_cmd = cmd->command_new();
  auto* aa = new google::protobuf::Any();
  aa->CopyFrom(client_cmd);
  // schedule command execution
  uint64_t task_id = 0;
  // Get trailing portion of protobuf type url
  auto name =
      split_string(split_string(client_cmd.type_url(), "/").back(), "\\.")
          .back();

  try {
    auto* a = new ISArgs{I, aa};

    auto c = I->cmd_manager_new().factory().make_command(name, a, C->socket());
    I->cmd_manager_new().enqueue_command(std::shared_ptr<command::Command>(c));
    task_id = c->task_id();
  } catch (const std::exception& e) {
    return reply_error(join_string({e.what(), name}));
  }
  // write status back
  Response presp;
  presp.set_code(RESPONSE_OK);
  RunCommandAck ack;
  ack.set_id(task_id);
  if (!presp.mutable_body()->PackFrom(ack)) {
    return reply_error("Packing ACK message failed");
  }
  result.resize(presp.ByteSizeLong());
  presp.SerializeToArray(result.data(), result.size());
  return presp;
}

// 1. parse command message from bytes
// 2. if cmd found, return ACK as usual and schedule for executin
// 3. pass command protobuf message as argument to command
// 4. execute command, store result in protobuf message's field
yrclient::Response InstrumentationService::process_request(
    connection::Connection* C, vecu8* bytes) {
  // read command from message
  Command cmd;
  if (!cmd.ParseFromArray(bytes->data(), bytes->size())) {
    return reply_error("Message parse error");
  }

  // execute parsed command & write result
  vecu8 result;
  switch (cmd.command_type()) {
    case yrclient::CLIENT_COMMAND: {
      return reply_error("Deprecated");
    } break;
    case yrclient::CLIENT_COMMAND_NEW:
      return handle_cmd_ng(this, C, bytes, &cmd);
    case yrclient::POLL: {
      return reply_error("Deprecated");
    } break;
    case yrclient::POLL_NEW: {
      try {
        return flush_results_new(C->socket());
      } catch (const std::out_of_range& e) {
        return reply_error(e.what());
      }
    }
    case yrclient::SHUTDOWN: {
      try {
        DPRINTF("shutdown signal");
        return reply_ok(on_shutdown_(this));
      } catch (std::bad_function_call& e) {
        return reply_error(e.what());
      }
    } break;
    default:
      break;
  }
  DPRINTF("something is wrong\n");
  return reply_error("Unknown command: " + std::to_string(cmd.command_type()));
}

vecu8 InstrumentationService::on_receive_bytes(connection::Connection* C,
                                               vecu8* bytes) {
  auto response = process_request(C, bytes);
  return to_vecu8(response);
}

void InstrumentationService::on_accept(connection::Connection* C) {
  // Create result queue
  cmd_manager_new().enqueue_builtin(command::CommandType::CREATE_QUEUE,
                                    C->socket());
}

void InstrumentationService::on_close(connection::Connection* C) {
  cmd_manager_new().enqueue_builtin(command::CommandType::DESTROY_QUEUE,
                                    C->socket());
}

void InstrumentationService::on_send_bytes(connection::Connection* C,
                                           vecu8* bytes) {
  (void)C;
  (void)bytes;
}

InstrumentationService::InstrumentationService(
    const unsigned int max_clients, const unsigned int port,
    std::function<std::string(InstrumentationService*)> on_shutdown)
    : server_(max_clients, port), on_shutdown_(on_shutdown) {
  server_.callbacks().receive_bytes = [this](auto* c, auto* b) {
    return this->on_receive_bytes(c, b);
  };
  server_.callbacks().send_bytes = [this](auto* c, auto* b) {
    this->on_send_bytes(c, b);
  };
  server_.callbacks().accept = [this](auto* c) { this->on_accept(c); };
  server_.callbacks().close = [this](auto* c) { this->on_close(c); };
  // TODO: avoid member function calls within ctor
  add_builtin_commands();
}

void InstrumentationService::add_builtin_commands() {
  auto cc = commands_yr::get_commands();
  for (auto& [name, fn] : *cc) {
    add_command_new(name, fn, &commands_builtin::command_deleter);
  }

  for (auto& [name, fn] : *commands_builtin::get_commands()) {
    add_command_new(name, fn, &commands_builtin::command_deleter);
  }
}

auto lock(std::mutex* m) { return std::unique_lock<std::mutex>(*m); }

void InstrumentationService::store_value(const std::string key, vecu8* data) {
  store_value(key, reinterpret_cast<void*>(data),
              [](void* data) { delete reinterpret_cast<vecu8*>(data); });
}

void InstrumentationService::store_value(const std::string key, void* data,
                                         deleter_t deleter) {
  auto lk = lock(&mut_storage_);
  DPRINTF("key=%s,val=%p\n", key.c_str(), data);
  storage_[key] = storage_val(data, deleter);
}

void* InstrumentationService::get_value(const std::string key) {
  auto lk = lock(&mut_storage_);
  return storage_.at(key).get();
}

void InstrumentationService::remove_value(const std::string key) {
  auto lk = lock(&mut_storage_);
  storage_.erase(key);
}

// TODO: remove?
aq_t<std::map<u8*, hook::Hook>*> InstrumentationService::aq_hooks() {
  return util::acquire(mut_hooks_, &hooks_);
}

aq_t<storage_t*> InstrumentationService::aq_storage() {
  return util::acquire(mut_storage_, &storage_);
}

InstrumentationService::~InstrumentationService() {}

std::tuple<yrclient::InstrumentationService*, std::vector<std::string>, void*>
yrclient::get_args(yrclient::IServiceArgs args) {
  return std::make_tuple(args.I, yrclient::split_string(*args.args),
                         args.result);
}
