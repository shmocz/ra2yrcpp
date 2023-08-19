#include "win_message.hpp"

typedef void* HWND;

#include <cstddef>

#include <errhandlingapi.h>
#include <minwindef.h>
#include <winbase.h>

#include <string>

std::string windows_utils::get_error_message(const int error_code) {
  char* buf = nullptr;
  std::size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf,
      0, NULL);

  std::string message(buf, size);
  LocalFree(buf);
  // Remove \r\n
  return message.substr(0, message.find("\r\n"));
}

unsigned long windows_utils::get_last_error() { return GetLastError(); }
