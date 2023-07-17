#include "instrumentation_service.hpp"

#include "protocol/protocol.hpp"

#include "asio_utils.hpp"
#include "connection.hpp"
#include "errors.hpp"
#include "logging.hpp"
#include "server.hpp"
#include "util_string.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <exception>
#include <iterator>
#include <stdexcept>
#include <utility>

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
  res.push_back(io_service_tid_.get());
  return res;
}

void InstrumentationService::create_hook(std::string name, u8* target,
                                         const size_t code_length) {
  std::unique_lock<std::mutex> lk(mut_hooks_);
  iprintf("name={},target={},size_bytes={}", name,
          reinterpret_cast<void*>(target), code_length);
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

static ra2yrproto::TextResponse text_response(const std::string message) {
  ra2yrproto::TextResponse E;
  E.mutable_message()->assign(message);
  return E;
}

ra2yrproto::PollResults InstrumentationService::flush_results(
    const u64 queue_id, const duration_t delay) {
  auto results = cmd_manager().flush_results(queue_id, delay, 0);
  ra2yrproto::PollResults P;
  auto* PR = P.mutable_result();
  while (!results.empty()) {
    auto item = results.back();
    auto* cmd_result =
        reinterpret_cast<ra2yrproto::CommandResult*>(item->result());
    auto* res = PR->add_results();
    res->set_command_id(cmd_result->command_id());
    res->mutable_result()->CopyFrom(cmd_result->result());
    if (item->result_code().get() == command::ResultCode::ERROR) {
      res->set_result_code(yrclient::RESPONSE_ERROR);
      res->set_error_message(*item->error_message());
    } else {
      res->set_result_code(yrclient::RESPONSE_OK);
    }
    results.pop_back();
  }

  return P;
}

static ra2yrproto::RunCommandAck handle_cmd(InstrumentationService* I,
                                            connection::Connection* C,
                                            ra2yrproto::Command* cmd) {
  // TODO: reduce amount of copies we make
  auto client_cmd = cmd->command();
  // schedule command execution
  auto is_args = new ISArgs;
  is_args->I = I;
  is_args->M.CopyFrom(client_cmd);

  // Get trailing portion of protobuf type url
  auto name = yrclient::split_string(client_cmd.type_url(), "/").back();
  ra2yrproto::RunCommandAck ack;

  auto c = std::shared_ptr<command::Command>(
      I->cmd_manager().factory().make_command(
          name,
          std::unique_ptr<void, void (*)(void*)>(
              is_args, [](auto d) { delete reinterpret_cast<ISArgs*>(d); }),
          C->socket()),
      [](auto* a) { delete a; });
  ack.set_id(c->task_id());
  I->cmd_manager().enqueue_command(c);

  // write status back
  ack.set_queue_id(C->socket());
  return ack;
}

// TODO: return just Response body/msg, not the whole Response
ra2yrproto::Response InstrumentationService::process_request(
    connection::Connection* C, vecu8* bytes, bool* is_json) {
  // read command from message
  ra2yrproto::Command cmd;
  if (!cmd.ParseFromArray(bytes->data(), bytes->size())) {
    if (!yrclient::from_json(*bytes, &cmd)) {
      throw std::runtime_error("Message parse error");
    } else {
      *is_json = true;
    }
  }

  // execute parsed command & write result
  switch (cmd.command_type()) {
    case ra2yrproto::CLIENT_COMMAND_OLD:
      throw std::runtime_error("Deprecated");
    case ra2yrproto::CLIENT_COMMAND: {
      auto ack = handle_cmd(this, C, &cmd);
      if (cmd.blocking()) {
        const u64 queue_id = (u64)C->socket();
        const auto timeout = cfg::POLL_BLOCKING_TIMEOUT;
        return yrclient::make_response(flush_results(queue_id, timeout));
      }
      return yrclient::make_response(ra2yrproto::RunCommandAck(ack));
    }
    case ra2yrproto::POLL: {
      return yrclient::make_response(flush_results(C->socket()));
    }
    case ra2yrproto::POLL_BLOCKING: {
      ra2yrproto::PollResults R;
      cmd.command().UnpackTo(&R);
      // TODO: correct check?
      const u64 queue_id =
          R.args().queue_id() > 0 ? R.args().queue_id() : (u64)C->socket();
      const auto timeout =
          R.args().IsInitialized()
              ? duration_t(static_cast<double>((u32)R.args().timeout()) /
                           1000.0)
              : cfg::POLL_BLOCKING_TIMEOUT;
      return yrclient::make_response(flush_results(queue_id, timeout));
    }
    case ra2yrproto::SHUTDOWN:
      return make_response(text_response(on_shutdown_(this)));
    default:
      throw std::runtime_error("unknown command: " +
                               std::to_string(cmd.command_type()));
  }
}

vecu8 InstrumentationService::on_receive_bytes(connection::Connection* C,
                                               vecu8* bytes) {
  ra2yrproto::Response R;
  bool is_json = false;
  try {
    R = process_request(C, bytes, &is_json);
    R.set_code(RESPONSE_OK);
  } catch (const std::exception& e) {
    eprintf("{}", e.what());
    R = yrclient::make_response(text_response(e.what()), RESPONSE_ERROR);
  }
  if (is_json) {
    return yrclient::to_bytes(yrclient::to_json(R));
  }
  return to_vecu8(R);
}

void InstrumentationService::on_accept(connection::Connection* C) {
  // Create result queue
  cmd_manager().enqueue_builtin(
      command::CommandType::CREATE_QUEUE, C->socket(),
      command::BuiltinArgs{.queue_size = cfg::RESULT_QUEUE_SIZE});
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

void InstrumentationService::store_value(
    const std::string key, std::unique_ptr<void, void (*)(void*)> d) {
  storage_[key] = std::move(d);
}

// FIXME: don't accept connections until fully initialized
// FIXME: server initialization must be atomic!
InstrumentationService::InstrumentationService(
    InstrumentationService::IServiceOptions opt,
    std::function<std::string(InstrumentationService*)> on_shutdown,
    std::function<void(InstrumentationService*)> extra_init)
    : on_shutdown_(on_shutdown),
      opts_(opt),
      server_(opt.max_clients, opt.port),
      io_service_tid_(0U),
      ws_proxy_object_(nullptr) {
  cmd_manager_.start();

  // Set server callbacks
  server_.callbacks().receive_bytes = [this](auto* c, auto* b) {
    return this->on_receive_bytes(c, b);
  };
  server_.callbacks().send_bytes = [this](auto* c, auto* b) {
    this->on_send_bytes(c, b);
  };
  server_.callbacks().accept = [this](auto* c) { this->on_accept(c); };
  server_.callbacks().close = [this](auto* c) { this->on_close(c); };

  // Start server and wait for it to become active
  // TODO: should the wait just happen inside the start()?
  server_.start();
  server_.state().wait(server::Server::STATE::ACTIVE);

  if (extra_init != nullptr) {
    extra_init(this);
  }

  // Create and start io_service manager
  io_service_ = std::make_unique<ra2yrcpp::asio_utils::IOService>();

  if (opt.ws_port < 1) {
    wrprintf("ws port is 0, disabling websocket proxy");
  } else {
    // TODO(shmocz): don't hardcode max clients
    ws_proxy_object_ = std::make_unique<ws_proxy_t>(
        ws_proxy_t::Options{opt.host, opt.port, opt.ws_port,
                            opt.max_clients + 4},
        io_service_->service_.get());
  }

  // Retrieve io_service thread id, so we know to not suspend it
  io_service_->post(
      [this]() { io_service_tid_.store(process::get_current_tid()); });
  io_service_tid_.wait_pred([](auto v) { return v != 0U; });
}

// FIXME: don't use
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

storage_t& InstrumentationService::storage() { return storage_; }

// TODO: remove?
util::acquire_t<std::map<u8*, hook::Hook>*> InstrumentationService::aq_hooks() {
  return util::acquire(mut_hooks_, &hooks_);
}

util::acquire_t<storage_t*, std::recursive_mutex>
InstrumentationService::aq_storage() {
  return util::acquire(mut_storage_, &storage_);
}

InstrumentationService::~InstrumentationService() {
  for (const auto& s : stop_handlers_) {
    s(nullptr);
  }
  if (ws_proxy_object_ != nullptr) {
    ws_proxy_object_->shutdown();
    // The IO service must be stopped beforehand, or we get UAF in the ws proxy
    io_service_ = nullptr;
  }

  server_.shutdown();
  cmd_manager_.shutdown();
}

const InstrumentationService::IServiceOptions& InstrumentationService::opts()
    const {
  return opts_;
}

yrclient::InstrumentationService* InstrumentationService::create(
    InstrumentationService::IServiceOptions O,
    std::map<std::string, command::Command::handler_t>* commands,
    std::function<std::string(yrclient::InstrumentationService*)> on_shutdown,
    std::function<void(InstrumentationService*)> extra_init) {
  auto* I = new yrclient::InstrumentationService(O, on_shutdown, extra_init);
  if (commands != nullptr) {
    for (auto& [name, fn] : *commands) {
      I->add_command(name, fn);
    }
  }
  return I;
}
