#include "instrumentation_service.hpp"

#include "protocol/protocol.hpp"

#include "asio_utils.hpp"
#include "command/command_manager.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "logging.hpp"
#include "protocol/helpers.hpp"
#include "util_string.hpp"

#include <fmt/core.h>
#include <google/protobuf/any.pb.h>

#include <exception>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>

using namespace ra2yrcpp;

ISCallback::ISCallback() : I(nullptr) {}

ISCallback::~ISCallback() {}

void ISCallback::add_to_hook(hook::Hook* h,
                             ra2yrcpp::InstrumentationService* I) {
  this->I = I;
  // TODO(shmocz): avoid using wrapper
  h->add_callback([this](hook::Hook* h, void* user_data,
                         X86Regs* state) { this->call(h, user_data, state); },
                  nullptr, name(), 0U);
}

std::vector<process::thread_id_t>
InstrumentationService::get_connection_threads() {
  std::vector<process::thread_id_t> res;
  res.push_back(io_service_tid_.get());
  return res;
}

void InstrumentationService::create_hook(const std::string& name,
                                         const std::uintptr_t target,
                                         const std::size_t code_length) {
  std::unique_lock<std::mutex> lk(mut_hooks_);
  iprintf("name={},target={:#x},size_bytes={}", name, target, code_length);
  if (hooks_.find(target) != hooks_.end()) {
    throw std::runtime_error(
        fmt::format("Can't overwrite existing hook (name={} address={})", name,
                    reinterpret_cast<void*>(target)));
  }
  auto tids = get_connection_threads();
  hooks_.try_emplace(target, target, code_length, name, tids, true);
}

cmd_manager_t& InstrumentationService::cmd_manager() { return cmd_manager_; }

hooks_t& InstrumentationService::hooks() { return hooks_; }

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
    auto* res = PR->add_results();
    res->set_command_id(item->task_id());
    res->mutable_result()->CopyFrom(item->command_data()->M);
    if (item->result_code().get() == ra2yrcpp::command::ResultCode::ERROR) {
      res->set_result_code(ra2yrcpp::RESPONSE_ERROR);
      res->set_error_message(item->error_message());
    } else {
      res->set_result_code(ra2yrcpp::RESPONSE_OK);
    }
    results.pop_back();
  }

  return P;
}

std::tuple<command_ptr_t, ra2yrproto::RunCommandAck> ra2yrcpp::handle_cmd(
    InstrumentationService* I, int queue_id, ra2yrproto::Command* cmd,
    bool discard_result) {
  // TODO: reduce amount of copies we make
  auto client_cmd = cmd->command();
  // Get trailing portion of protobuf type url
  auto name = ra2yrcpp::split_string(client_cmd.type_url(), "/").back();
  ra2yrproto::RunCommandAck ack;

  auto c = I->cmd_manager().make_command(
      name, ra2yrcpp::command::ISArg{reinterpret_cast<void*>(I), client_cmd},
      queue_id);
  ack.set_id(c->task_id());
  c->discard_result().store(discard_result);
  I->cmd_manager().enqueue_command(c);

  // write status back
  ack.set_queue_id(queue_id);
  return std::make_tuple(c, ack);
}

// TODO: return just Response body/msg, not the whole Response
ra2yrproto::Response InstrumentationService::process_request(
    const int socket_id, vecu8* bytes, bool* is_json) {
  // read command from message
  ra2yrproto::Command cmd;
  if (!cmd.ParseFromArray(bytes->data(), bytes->size())) {
    if (!ra2yrcpp::protocol::from_json(*bytes, &cmd)) {
      throw std::runtime_error("Message parse error");
    } else {
      *is_json = true;
    }
  }

  // execute parsed command & write result
  switch (cmd.command_type()) {
    case ra2yrproto::CLIENT_COMMAND: {
      auto [cptr, ack] = handle_cmd(this, socket_id, &cmd);
      if (cmd.blocking()) {
        const u64 queue_id = (u64)socket_id;
        const auto timeout = cfg::POLL_BLOCKING_TIMEOUT;
        return ra2yrcpp::make_response(flush_results(queue_id, timeout));
      }
      return ra2yrcpp::make_response(ra2yrproto::RunCommandAck(ack));
    }
    case ra2yrproto::POLL: {
      return ra2yrcpp::make_response(flush_results(socket_id));
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
      return ra2yrcpp::make_response(flush_results(queue_id, timeout));
    }
    case ra2yrproto::SHUTDOWN:
      return make_response(text_response(on_shutdown_(this)));
    default:
      throw std::runtime_error("unknown command: " +
                               std::to_string(cmd.command_type()));
  }
}

std::string InstrumentationService::on_shutdown() {
  if (on_shutdown_ != nullptr) {
    return on_shutdown_(this);
  }
  return "";
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
    R = ra2yrcpp::make_response(text_response(e.what()), RESPONSE_ERROR);
  }
  if (is_json) {
    return ra2yrcpp::to_bytes(ra2yrcpp::protocol::to_json(R));
  }
  return to_vecu8(R);
}

static void on_accept(InstrumentationService* I, const int socket_id) {
  // Create result queue
  // TODO(shmocz): can block here
  (void)I->cmd_manager().execute_create_queue(socket_id,
                                              cfg::RESULT_QUEUE_SIZE);
}

static void on_close(InstrumentationService* I, const int socket_id) {
  (void)I->cmd_manager().execute_destroy_queue(socket_id);
}

InstrumentationService::InstrumentationService(
    InstrumentationService::Options opt,
    std::function<std::string(InstrumentationService*)> on_shutdown,
    std::function<void(InstrumentationService*)> extra_init)
    : opts_(opt),
      on_shutdown_(on_shutdown),
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

void* InstrumentationService::get_value(const std::string key,
                                        const bool acquire) {
  if (acquire) {
    auto [lk, s] = aq_storage();
    return s->at(key).get();
  }
  return storage_.at(key).get();
}

storage_t& InstrumentationService::storage() { return storage_; }

util::acquire_t<hooks_t> InstrumentationService::aq_hooks() {
  return util::acquire(&hooks_, &mut_hooks_);
}

util::acquire_t<storage_t, std::recursive_mutex>
InstrumentationService::aq_storage() {
  return util::acquire(&storage_, &mut_storage_);
}

InstrumentationService::~InstrumentationService() {
  ws_server_->shutdown();
  cmd_manager_.shutdown();
}

const InstrumentationService::Options& InstrumentationService::opts() const {
  return opts_;
}

ra2yrcpp::InstrumentationService* InstrumentationService::create(
    InstrumentationService::Options O,
    std::map<std::string, cmd_t::handler_t> commands,
    std::function<std::string(ra2yrcpp::InstrumentationService*)> on_shutdown,
    std::function<void(InstrumentationService*)> extra_init) {
  auto* I = new ra2yrcpp::InstrumentationService(O, on_shutdown, extra_init);
  for (auto& [name, fn] : commands) {
    I->cmd_manager().add_command(name, fn);
  }
  return I;
}
