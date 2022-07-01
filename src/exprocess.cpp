#include "exprocess.hpp"

#include "errors.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace exprocess;

ExProcess::ExProcess(const std::string cmdline) : cmdline_(cmdline) {
#ifdef _WIN32
  memset(&si_, 0, sizeof(si_));
  si_.cb = sizeof(si_);
  memset(&pi_, 0, sizeof(pi_));
  if (!CreateProcess(nullptr,  // No module name (use command line)
                     const_cast<char*>(cmdline_.c_str()),  // Command line
                     nullptr,  // Process handle not inheritable
                     nullptr,  // Thread handle not inheritable
                     FALSE,    // Set handle inheritance to FALSE
                     0,        // No creation flags
                     nullptr,  // Use parent's environment block
                     nullptr,  // Use parent's starting directory
                     &si_,     // Pointer to STARTUPINFO structure
                     &pi_)     // Pointer to PROCESS_INFORMATION structure
  ) {
    throw yrclient::system_error("Failed to create process");
  }

#else
#error Not implemented
#endif
}

void* ExProcess::handle() { return pi_.hProcess; }

void ExProcess::join() {
  if (WaitForSingleObject(handle(), INFINITE) == WAIT_FAILED) {
    throw yrclient::system_error("WaitForSingleObject");
  }
}

ExProcess::~ExProcess() {
#ifdef _WIN32
  CloseHandle(pi_.hProcess);
  CloseHandle(pi_.hThread);
#else
#endif
}
