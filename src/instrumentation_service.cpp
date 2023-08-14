#include "instrumentation_service.hpp"

#include "protocol/protocol.hpp"

#include "asio_utils.hpp"
#include "errors.hpp"
#include "logging.hpp"
#include "util_string.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <exception>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>

using namespace yrclient;

ISCallback::ISCallback() : I(nullptr) {}

ISCallback::~ISCallback() {}

void ISCallback::add_to_hook(hook::Hook* h,
                             yrclient::InstrumentationService* I) {
  this->I = I;
  // TODO(shmocz): avoid using wrapper
  h->add_callback([this](hook::Hook* h, void* user_data,
                         X86Regs* state) { this->call(h, user_data, state); },
                  nullptr, name(), 0U);
}

void InstrumentationService::add_command(std::string name,
                                         command::Command::handler_t fn) {
  cmd_manager_.factory().add_entry(name, fn);
}

std::vector<process::thread_id_t>
InstrumentationService::get_connection_threads() {
  std::vector<process::thread_id_t> res;
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
                                            const int queue_id,
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
          queue_id),
      [](auto* a) { delete a; });
  ack.set_id(c->task_id());
  I->cmd_manager().enqueue_command(c);

  // write status back
  ack.set_queue_id(queue_id);
  return ack;
}

// TODO: return just Response body/msg, not the whole Response
ra2yrproto::Response InstrumentationService::process_request(
    const int socket_id, vecu8* bytes, bool* is_json) {
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
    case ra2yrproto::CLIENT_COMMAND: {
      auto ack = handle_cmd(this, socket_id, &cmd);
      if (cmd.blocking()) {
        const u64 queue_id = (u64)socket_id;
        const auto timeout = cfg::POLL_BLOCKING_TIMEOUT;
        return yrclient::make_response(flush_results(queue_id, timeout));
      }
      return yrclient::make_response(ra2yrproto::RunCommandAck(ack));
    }
    case ra2yrproto::POLL: {
      return yrclient::make_response(flush_results(socket_id));
    }
    case ra2yrproto::POLL_BLOCKING: {
      ra2yrproto::PollResults R;
      cmd.command().UnpackTo(&R);
      // TODO: correct check?
      const u64 queue_id =
          R.args().queue_id() > 0 ? R.args().queue_id() : (u64)socket_id;
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

static vecu8 on_receive_bytes(InstrumentationService* I, const int socket_id,
                              vecu8* bytes) {
  ra2yrproto::Response R;
  bool is_json = false;
  try {
    R = I->process_request(socket_id, bytes, &is_json);
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

static void on_accept(InstrumentationService* I, const int socket_id) {
  // Create result queue
  // FIXME: can block here
  I->cmd_manager().enqueue_builtin(
      command::CommandType::CREATE_QUEUE, socket_id,
      command::BuiltinArgs{.queue_size = cfg::RESULT_QUEUE_SIZE});
}

static void on_close(InstrumentationService* I, const int socket_id) {
  I->cmd_manager().enqueue_builtin(command::CommandType::DESTROY_QUEUE,
                                   socket_id);
}

void InstrumentationService::store_value(
    const std::string key, std::unique_ptr<void, void (*)(void*)> d) {
  storage_[key] = std::move(d);
}

InstrumentationService::InstrumentationService(
    InstrumentationService::Options opt,
    std::function<std::string(InstrumentationService*)> on_shutdown,
    std::function<void(InstrumentationService*)> extra_init)
    : on_shutdown_(on_shutdown),
      opts_(opt),
      io_service_tid_(0U),
      ws_server_(nullptr) {
  cmd_manager_.start();

  if (extra_init != nullptr) {
    extra_init(this);
  }

  // Create and start io_service manager
  io_service_ = std::make_unique<ra2yrcpp::asio_utils::IOService>();
  // Retrieve io_service thread id, so we know to not suspend it
  io_service_->post(
      [this]() { io_service_tid_.store(process::get_current_tid()); });
  io_service_tid_.wait_pred([](auto v) { return v != 0U; });

  {
    WebsocketServer::Callbacks cb{nullptr, nullptr, nullptr};
    cb.accept = [this](int id) { on_accept(this, id); };
    cb.close = [this](int id) { on_close(this, id); };
    cb.receive = [this](int id, auto* msg) {
      // TODO(shmocz): avoid copy
      vecu8 bt = to_bytes(*msg);
      auto resp = on_receive_bytes(this, id, &bt);
      return to_string(resp);
    };
    ws_server_ = ra2yrcpp::websocket_server::create_server(
        opts_.server, io_service_.get(), cb);
    ws_server_->start();
  }
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
  ws_server_->shutdown();
  cmd_manager_.shutdown();
}

const InstrumentationService::Options& InstrumentationService::opts() const {
  return opts_;
}

yrclient::InstrumentationService* InstrumentationService::create(
    InstrumentationService::Options O,
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
