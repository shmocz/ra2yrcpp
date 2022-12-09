#include "instrumentation_service.hpp"

using namespace yrclient;

yrclient::ISCallback::ISCallback() : I(nullptr) {}

void yrclient::ISCallback::call(hook::Hook* h, void* data, X86Regs* state) {
  (void)h;
  this->I = static_cast<yrclient::InstrumentationService*>(data);
  this->cpu_state = state;
  do_call(this->I);
  this->cpu_state = nullptr;
}

std::string yrclient::ISCallback::name() { throw yrclient::not_implemented(); }

std::string yrclient::ISCallback::target() {
  throw yrclient::not_implemented();
}

void InstrumentationService::add_command(std::string name,
                                         command::Command::handler_t fn) {
  cmd_manager_.factory().add_entry(name, fn);
}

std::vector<process::thread_id_t>
InstrumentationService::get_connection_threads() {
  std::vector<process::thread_id_t> res;
  auto& C = server().connections();
  std::transform(C.begin(), C.end(), std::back_inserter(res),
                 [](const auto& ctx) { return ctx->thread_id; });
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
                     name, tids, true);
}

command::CommandManager& InstrumentationService::cmd_manager() {
  return cmd_manager_;
}

server::Server& InstrumentationService::server() { return server_; }

std::map<u8*, hook::Hook>& InstrumentationService::hooks() { return hooks_; }

template <typename T>
ra2yrproto::Response* reply_body(ra2yrproto::Response* R, const T& body) {
  if (!R->mutable_body()->PackFrom(body)) {
    throw yrclient::general_error("Couldn't pack response body");
  }
  return R;
}

ra2yrproto::Response reply_error(std::string message) {
  ra2yrproto::Response R;
  ra2yrproto::TextResponse E;
  E.mutable_message()->assign(message);
  // E.set_error_message(message);
  R.set_code(RESPONSE_ERROR);

  return *reply_body(&R, E);
}

ra2yrproto::Response reply_ok(std::string message) {
  ra2yrproto::Response R;
  ra2yrproto::TextResponse T;
  T.mutable_message()->assign(message);
  R.set_code(RESPONSE_OK);
  return *reply_body(&R, T);
}

ra2yrproto::Response InstrumentationService::flush_results(
    const u64 queue_id, const std::chrono::milliseconds delay) {
  auto results = cmd_manager().flush_results(queue_id, delay, 0);
  ra2yrproto::Response R;
  ra2yrproto::PollResults P;
  auto* PR = P.mutable_result();
  while (!results.empty()) {
    auto item = results.back();
    auto* cmd_result = as<ra2yrproto::CommandResult*>(item->result());
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

// TODO: static as separate commit
static ra2yrproto::Response handle_cmd_ng(InstrumentationService* I,
                                          connection::Connection* C,
                                          vecu8* bytes,
                                          ra2yrproto::Command* cmd) {
  (void)bytes;
  using yrclient::split_string;
  // TODO: reduce amount of copies we make
  auto client_cmd = cmd->command();
  // schedule command execution
  uint64_t task_id = 0;
  const uint64_t queue_id = C->socket();
  auto is_args = new ISArgs;
  is_args->I = I;
  is_args->M.CopyFrom(client_cmd);

  // Get trailing portion of protobuf type url
  auto name = split_string(client_cmd.type_url(), "/").back();

  try {
    auto c = I->cmd_manager().factory().make_command(
        name,
        std::unique_ptr<void, void (*)(void*)>(
            is_args, [](auto d) { delete reinterpret_cast<ISArgs*>(d); }),
        queue_id);
    I->cmd_manager().enqueue_command(std::shared_ptr<command::Command>(c));
    task_id = c->task_id();
  } catch (const std::exception& e) {
    return reply_error(join_string({e.what(), name}));
  }
  // write status back (FIXME: use make_response)
  ra2yrproto::Response presp;
  presp.set_code(RESPONSE_OK);
  ra2yrproto::RunCommandAck ack;
  ack.set_id(task_id);
  ack.set_queue_id(queue_id);
  if (!presp.mutable_body()->PackFrom(ack)) {
    return reply_error("Packing ACK message failed");
  }
  vecu8 result;
  result.resize(presp.ByteSizeLong());
  presp.SerializeToArray(result.data(), result.size());
  return presp;
}

// 1. parse command message from bytes
// 2. if cmd found, return ACK as usual and schedule for executin
// 3. pass command protobuf message as argument to command
// 4. execute command, store result in protobuf message's field
ra2yrproto::Response InstrumentationService::process_request(
    connection::Connection* C, vecu8* bytes) {
  // read command from message
  ra2yrproto::Command cmd;
  if (!cmd.ParseFromArray(bytes->data(), bytes->size())) {
    return reply_error("Message parse error");
  }

  // execute parsed command & write result
  switch (cmd.command_type()) {
    case ra2yrproto::CLIENT_COMMAND_OLD: {
      return reply_error("Deprecated");
    } break;
    case ra2yrproto::CLIENT_COMMAND:
      return handle_cmd_ng(this, C, bytes, &cmd);
    case ra2yrproto::POLL: {
      try {
        return flush_results(C->socket());
      } catch (const std::out_of_range& e) {
        return reply_error(e.what());
      }
    }
    case ra2yrproto::POLL_BLOCKING: {
      try {
        ra2yrproto::PollResults R;
        cmd.command().UnpackTo(&R);
        // TODO: correct check?
        const u64 queue_id =
            R.args().queue_id() > 0 ? R.args().queue_id() : (u64)C->socket();
        const auto timeout = std::chrono::milliseconds(
            R.args().IsInitialized() ? (u32)R.args().timeout()
                                     : cfg::POLL_BLOCKING_TIMEOUT_MS);
        dprintf("queue_id={},timeout={}", queue_id, (u64)timeout.count());
        // TODO: race condition? what if flush occurs after destroying cmd
        // manager? server should've been shut down before command manager, so
        // shouldnt be possible
        return flush_results(queue_id, timeout);
      } catch (const std::exception& e) {
        return reply_error(e.what());
      }
    }
    case ra2yrproto::SHUTDOWN: {
      try {
        dprintf("shutdown signal");
        return reply_ok(on_shutdown_(this));
      } catch (std::bad_function_call& e) {
        return reply_error(e.what());
      }
    } break;
    default:
      break;
  }
  eprintf("something is wrong");
  return reply_error("Unknown command: " + std::to_string(cmd.command_type()));
}

vecu8 InstrumentationService::on_receive_bytes(connection::Connection* C,
                                               vecu8* bytes) {
  auto response = process_request(C, bytes);
  return to_vecu8(response);
}

void InstrumentationService::on_accept(connection::Connection* C) {
  // Create result queue
  cmd_manager().enqueue_builtin(command::CommandType::CREATE_QUEUE, C->socket(),
                                command::BuiltinArgs{.queue_size = 32});
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
  dprintf("key={},val={}", key.c_str(), data);
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

// TODO: remove?
aq_t<std::map<u8*, hook::Hook>*> InstrumentationService::aq_hooks() {
  return util::acquire(mut_hooks_, &hooks_);
}

aq_t<storage_t*> InstrumentationService::aq_storage() {
  return util::acquire(mut_storage_, &storage_);
}

InstrumentationService::~InstrumentationService() {}
