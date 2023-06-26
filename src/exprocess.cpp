#include "exprocess.hpp"

#include "errors.hpp"
#include "logging.hpp"

#include <synchapi.h>
#include <windows.h>

using namespace exprocess;

ExProcess::ExProcess(const std::string cmdline, const std::string directory)
    : cmdline_(cmdline), directory_(directory) {
#ifdef _WIN32
  memset(&si_, 0, sizeof(si_));
  si_.cb = sizeof(si_);
  memset(&pi_, 0, sizeof(pi_));
  if (!CreateProcess(
          nullptr,  // No module name (use command line)
          const_cast<char*>(cmdline_.c_str()),  // Command line
          nullptr,  // Process handle not inheritable
          nullptr,  // Thread handle not inheritable
          FALSE,    // Set handle inheritance to FALSE
          0,        // No creation flags
          nullptr,  // Use parent's environment block
          (directory_.empty()
               ? nullptr
               : directory_.c_str()),  // Use parent's starting directory
          &si_,                        // Pointer to STARTUPINFO structure
          &pi_)  // Pointer to PROCESS_INFORMATION structure
  ) {
    throw yrclient::system_error("Failed to create process");
  }

#else
#error Not implemented
#endif
}

void* ExProcess::handle() { return pi_.hProcess; }

void ExProcess::join() {
  if (WaitForSingleObject(handle(), 5000) == WAIT_FAILED) {
    throw yrclient::system_error("WaitForSingleObject");
  }
}

unsigned long ExProcess::pid() const { return pi_.dwProcessId; }

ExProcess::~ExProcess() {
#ifdef _WIN32
  (void)TerminateProcess(pi_.hProcess, 0);
  if (!(WaitForSingleObject(pi_.hProcess, 5000) == WAIT_OBJECT_0)) {
    eprintf("error with WaitForSingleProcess");
  }
  CloseHandle(pi_.hProcess);
  CloseHandle(pi_.hThread);
#else
#endif
}
