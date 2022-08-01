#include "instrumentation_service.hpp"

using namespace yrclient;

void yrclient::ISCallback::call(hook::Hook* h, void* data, X86Regs* state) {
  (void)h;
  (void)state;
  auto* I = static_cast<yrclient::InstrumentationService*>(data);
  do_call(I);
}

std::string yrclient::ISCallback::name() { throw yrclient::not_implemented(); }
std::string yrclient::ISCallback::target() {
  throw yrclient::not_implemented();
}

void InstrumentationService::add_command_new(
    std::string name, command::Command::handler_t fn,
    command::Command::deleter_t deleter) {
  cmd_manager_.factory().add_entry(name, fn, deleter);
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
  if (hooks_.find(target) != hooks_.end()) {
    throw yrclient::general_error(
        fmt::format("Can't overwrite existing hook (name={} address={})", name,
                    reinterpret_cast<void*>(target)));
  }
  auto tids = get_connection_threads();
  hooks_.try_emplace(target, reinterpret_cast<addr_t>(target), code_length,
                     name, tids);
}

command::CommandManager& InstrumentationService::cmd_manager() {
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

yrclient::Response InstrumentationService::flush_results(
    const u64 queue_id, const std::chrono::milliseconds delay) {
  auto results = cmd_manager().flush_results(queue_id, delay, 0);
  yrclient::Response R;
  yrclient::PollResults P;
  auto* PR = P.mutable_result();
  while (!results.empty()) {
    auto item = results.back();
    auto* cmd_result = as<CommandResult*>(item->result());
    auto* res = PR->add_results();
    res->set_command_id(cmd_result->command_id());
    res->mutable_result()->CopyFrom(cmd_result->result());
    if (*item->result_code() == command::ResultCode::ERROR) {
      res->set_result_code(yrclient::RESPONSE_ERROR);
      res->set_error_message(*item->error_message());
    } else {
      res->set_result_code(yrclient::RESPONSE_OK);
    }
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
  auto client_cmd = cmd->command();
  auto* aa = new google::protobuf::Any();
  aa->CopyFrom(client_cmd);
  // schedule command execution
  uint64_t task_id = 0;
  const uint64_t queue_id = C->socket();
  // Get trailing portion of protobuf type url
  auto name =
      split_string(split_string(client_cmd.type_url(), "/").back(), "\\.")
          .back();

  try {
    auto c = I->cmd_manager().factory().make_command(name, new ISArgs{I, aa},
                                                     queue_id);
    I->cmd_manager().enqueue_command(std::shared_ptr<command::Command>(c));
    task_id = c->task_id();
  } catch (const std::exception& e) {
    return reply_error(join_string({e.what(), name}));
  }
  // write status back
  Response presp;
  presp.set_code(RESPONSE_OK);
  RunCommandAck ack;
  ack.set_id(task_id);
  ack.set_queue_id(queue_id);
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
    case yrclient::CLIENT_COMMAND_OLD: {
      return reply_error("Deprecated");
    } break;
    case yrclient::CLIENT_COMMAND:
      return handle_cmd_ng(this, C, bytes, &cmd);
    case yrclient::POLL: {
      try {
        return flush_results(C->socket());
      } catch (const std::out_of_range& e) {
        return reply_error(e.what());
      }
    }
    case yrclient::POLL_BLOCKING: {
      try {
        yrclient::PollResults R;
        cmd.command().UnpackTo(&R);
        // TODO: correct check?
        const u64 queue_id =
            R.args().queue_id() > 0 ? R.args().queue_id() : (u64)C->socket();
        const auto timeout = std::chrono::milliseconds(
            R.args().IsInitialized() ? (u32)R.args().timeout()
                                     : cfg::POLL_BLOCKING_TIMEOUT_MS);
        DPRINTF("queue_id=%llu,timeout=%llu\n", queue_id, (u64)timeout.count());
        // TODO: race condition? what if flush occurs after destroying cmd
        // manager? server should've been shut down before command manager, so
        // shouldnt be possible
        return flush_results(queue_id, timeout);
      } catch (const std::exception& e) {
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
  cmd_manager().enqueue_builtin(command::CommandType::CREATE_QUEUE,
                                C->socket());
}

void InstrumentationService::on_close(connection::Connection* C) {
  cmd_manager().enqueue_builtin(command::CommandType::DESTROY_QUEUE,
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
    : on_shutdown_(on_shutdown), server_(max_clients, port) {
  server_.callbacks().receive_bytes = [this](auto* c, auto* b) {
    return this->on_receive_bytes(c, b);
  };
  server_.callbacks().send_bytes = [this](auto* c, auto* b) {
    this->on_send_bytes(c, b);
  };
  server_.callbacks().accept = [this](auto* c) { this->on_accept(c); };
  server_.callbacks().close = [this](auto* c) { this->on_close(c); };
}

void InstrumentationService::store_value(const std::string key, vecu8* data) {
  store_value(key, reinterpret_cast<void*>(data),
              [](void* data) { delete reinterpret_cast<vecu8*>(data); });
}

void InstrumentationService::store_value(const std::string key, void* data,
                                         deleter_t deleter) {
  DPRINTF("key=%s,val=%p\n", key.c_str(), data);
  storage_[key] = storage_val(data, deleter);
}

void* InstrumentationService::get_value(const std::string key,
                                        const bool acquire) {
  if (acquire) {
    auto [lk, s] = aq_storage();
    return s->at(key).get();
  }
  return storage_.at(key).get();
}

void InstrumentationService::remove_value(const std::string key) {
  auto [lk, s] = aq_storage();
  s->erase(key);
}

// TODO: remove?
aq_t<std::map<u8*, hook::Hook>*> InstrumentationService::aq_hooks() {
  return util::acquire(mut_hooks_, &hooks_);
}

aq_t<storage_t*> InstrumentationService::aq_storage() {
  return util::acquire(mut_storage_, &storage_);
}

InstrumentationService::~InstrumentationService() {}
