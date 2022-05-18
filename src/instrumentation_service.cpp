#include "instrumentation_service.hpp"
#include "commands_builtin.hpp"
#include "commands_yr.hpp"

using namespace yrclient;

void InstrumentationService::add_command(std::string name,
                                         IServiceCommand cmd) {
  auto f = [cmd, this](void* p) -> std::unique_ptr<vecu8> {
    IServiceArgs aa{this, reinterpret_cast<std::string*>(p), nullptr};
    return cmd(aa);
  };
  cmd_manager_.add_command(name, f);
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

command_manager::CommandManager& InstrumentationService::cmd_manager() {
  return cmd_manager_;
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
  yrclient::ErrorResponse E;
  E.mutable_error_message()->assign(message);
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

yrclient::Response InstrumentationService::flush_results(const size_t id) {
  yrclient::Response R;
  auto [lock, res_queue] = cmd_manager().result_queue();
  yrclient::CommandPollResult P;
  auto& c_queue = res_queue->at(id);
  while (!c_queue.empty()) {
    auto& item = c_queue.front();
    auto R = P.add_results();
    R->set_result_code(RESPONSE_OK);
    if (item.code == command_manager::CommandResultCode::COMMAND_ERROR) {
      R->set_result_code(RESPONSE_ERROR);
    }
    auto msg = yrclient::to_string(*item.data);
    R->mutable_data()->assign(item.data->begin(), item.data->end());
    c_queue.pop();
  }

  R.mutable_body()->PackFrom(P);
  return R;
}

// TODO: use switch-case or std::map with lambdas
yrclient::Response InstrumentationService::process_request(
    connection::Connection* C, vecu8* bytes) {
  yrclient::Response presp;
  // read command from message
  Command cmd;
  if (!cmd.ParseFromArray(bytes->data(), bytes->size())) {
    return reply_error("Message parse error");
  }

  // execute parsed command & write result
  vecu8 result;
  if (cmd.command_type() == yrclient::CLIENT_COMMAND) {
    auto client_cmd = cmd.client_command();
    // schedule command execution
    uint64_t task_id = 0;
    try {
      task_id = cmd_manager().run_command(
          C->socket(), client_cmd.name().c_str(), client_cmd.args());
    } catch (const std::exception& e) {
      return reply_error(e.what());
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
  } else if (cmd.command_type() == yrclient::POLL) {
    // pop entries from result queue and send them back
    try {
      return flush_results(C->socket());
    } catch (const std::out_of_range& e) {
      return reply_error(e.what());
    }
  } else if (cmd.command_type() == yrclient::SHUTDOWN) {
    try {
      DPRINTF("shutdown signal");
      return reply_ok(on_shutdown_(this));
    } catch (std::bad_function_call& e) {
      return reply_error(e.what());
    }
  } else {
    return reply_error("Unknown command: " +
                       std::to_string(cmd.command_type()));
  }
  return presp;
}

vecu8 InstrumentationService::on_receive_bytes(connection::Connection* C,
                                               vecu8* bytes) {
  auto response = process_request(C, bytes);
  return to_vecu8(response);
}

void InstrumentationService::on_accept(connection::Connection* C) {
  // Create result queue
  cmd_manager().run_command(cmd_manager().create_queue(C->socket()));
}

void InstrumentationService::on_close(connection::Connection* C) {
  cmd_manager().run_command(cmd_manager().destroy_queue(C->socket()));
}

void InstrumentationService::on_send_bytes(connection::Connection* C,
                                           vecu8* bytes) {}

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
  auto cc = commands_builtin::get_commands();
  for (auto& [name, fn] : *cc) {
    add_command(name, fn);
  }
  cc = commands_yr::get_commands();
  for (auto& [name, fn] : *cc) {
    add_command(name, fn);
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

std::tuple<std::unique_lock<std::mutex>, std::map<u8*, hook::Hook>*>
InstrumentationService::aq_hooks() {
  return std::make_tuple(std::unique_lock<std::mutex>(mut_hooks_), &hooks_);
}

InstrumentationService::~InstrumentationService() {}

std::tuple<yrclient::InstrumentationService*, std::vector<std::string>, void*>
yrclient::get_args(yrclient::IServiceArgs args) {
  return std::make_tuple(args.I, yrclient::split_string(*args.args),
                         args.result);
}
