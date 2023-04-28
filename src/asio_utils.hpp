#pragma once
#include "utility/memtools.hpp"

#include <functional>
#include <memory>
#include <thread>

namespace ra2yrcpp {
namespace asio_utils {

class IOService {
 public:
  IOService();
  ~IOService();
  void post(std::function<void()> fn);

  std::unique_ptr<void, void (*)(void*)> service_;
  std::unique_ptr<void, void (*)(void*)> work_guard_;
  std::thread main_thread_;
};
}  // namespace asio_utils
}  // namespace ra2yrcpp
