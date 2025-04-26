#include "win32/windows_debug.hpp"

#include "win32/windows_utils.hpp"

#include <cstdlib>

#include <windows.h>

#include <dbghelp.h>
#include <tlhelp32.h>
#include <winternl.h>

struct NtCall {
  HMODULE m;

  explicit NtCall(HMODULE m) : m(m) {}

  template <typename... Args>
  constexpr auto call(const char* name, Args... args) const {
    auto const fn = reinterpret_cast<int(__stdcall*)(Args...)>(
        windows_utils::get_proc_address(name, m));
    return fn(args...);
  }
};

// Based on `TryDetachFromDebugger` found in CnCNet `yrpp-spawner`
// cppcheck-suppress-begin cstyleCast
bool windows_utils::debugger_detach() {
  auto GetDebuggerProcessId = [](DWORD dwSelfProcessId) -> DWORD {
    DWORD dwParentProcessId = -1;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(2, 0);
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    Process32First(hSnapshot, &pe32);
    do {
      if (pe32.th32ProcessID == dwSelfProcessId) {
        dwParentProcessId = pe32.th32ParentProcessID;
        break;
      }
    } while (Process32Next(hSnapshot, &pe32));
    CloseHandle(hSnapshot);
    return dwParentProcessId;
  };

  HMODULE hModule = LoadLibrary("ntdll.dll");
  auto const C = NtCall(hModule);
  if (hModule != nullptr) {
    HANDLE hDebug;
    HANDLE hCurrentProcess = GetCurrentProcess();
    NTSTATUS status = C.call("NtQueryInformationProcess", hCurrentProcess,
                             (PROCESSINFOCLASS)30, &hDebug, sizeof(HANDLE), 0);
    if (0 <= status) {
      ULONG killProcessOnExit = FALSE;
      status = C.call("NtSetInformationDebugObject", hDebug, 1,
                      &killProcessOnExit, sizeof(ULONG), NULL);
      if (0 <= status) {
        const auto pid = GetDebuggerProcessId(GetProcessId(hCurrentProcess));
        status = C.call("NtRemoveProcessDebug", hCurrentProcess, hDebug);
        if (0 <= status) {
          HANDLE hDbgProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
          if (INVALID_HANDLE_VALUE != hDbgProcess) {
            BOOL ret = TerminateProcess(hDbgProcess, EXIT_SUCCESS);
            CloseHandle(hDbgProcess);
            return ret;
          }
        }
      }
      C.call("NtClose", hDebug);
    }
    FreeLibrary(hModule);
  }

  return false;
}

// cppcheck-suppress-end cstyleCast
