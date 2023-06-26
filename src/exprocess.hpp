#pragma once

#include <processthreadsapi.h>
#include <string>

namespace exprocess {

class ExProcess {
 private:
  const std::string cmdline_;
  const std::string directory_;

#ifdef _WIN32
  STARTUPINFOA si_;
  PROCESS_INFORMATION pi_;
#endif

 public:
  explicit ExProcess(const std::string cmdline,
                     const std::string directory = "");
  void* handle();
  void join();
  unsigned long pid() const;
  ~ExProcess();
};

}  // namespace exprocess
