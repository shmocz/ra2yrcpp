#pragma once
#include <processthreadsapi.h>
#include <string>

namespace exprocess {

class ExProcess {
 private:
  const std::string cmdline_;

#ifdef _WIN32
  STARTUPINFO si_;
  PROCESS_INFORMATION pi_;
#endif

 public:
  explicit ExProcess(const std::string cmdline);
  void* handle();
  void join();
  ~ExProcess();
};

}  // namespace exprocess
