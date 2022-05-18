#pragma once

#include <functional>
#include <condition_variable>
#include <thread>
#include <string>
#include <mutex>

namespace context {

///
/// Class which owns an object that implements RAII pattern. Listens for a
/// shutdown signal from the object, then deletes it.
///
class Context {
 public:
  Context();
  void set_on_signal(std::function<std::string(Context*)> cb_on_signal);
  std::string on_signal();
  void wait_signal();
  void join();
  void*& data();
  std::function<void(Context*)>& deleter();

 private:
  void* data_;
  bool signaled;
  std::function<std::string(Context*)> on_signal_;
  std::function<void(Context*)> deleter_;
  std::thread t_;
  std::mutex m;
  std::condition_variable v;
};

}  // namespace context
