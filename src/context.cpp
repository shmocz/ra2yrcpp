#include "context.hpp"

using namespace context;

Context::Context() : data_(nullptr), signaled(false) {
  t_ = std::thread(&Context::wait_signal, this);
  deleter_ = nullptr;
  t_.detach();
}

void Context::join() {
  std::unique_lock<std::mutex> lk(m);
  v.wait(lk, [this] { return signaled; });
}

void*& Context::data() { return data_; }

std::function<void(Context*)>& Context::deleter() { return deleter_; }

void Context::set_on_signal(std::function<std::string(Context*)> cb_on_signal) {
  on_signal_ = cb_on_signal;
}

std::string Context::on_signal() {
  std::unique_lock<std::mutex> lk(m);
  signaled = true;
  v.notify_all();
  return "";
}

void Context::wait_signal() {
  join();
  deleter()(this);
}
