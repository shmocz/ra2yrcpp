#include "errors.hpp"

#include <string>

#ifdef _WIN32
#include "win32/win_message.hpp"
#elif __linux__
#include <cstring>

#include <cerrno>
#endif

using namespace ra2yrcpp;

int ra2yrcpp::get_last_error() {
#ifdef _WIN32
  return static_cast<int>(windows_utils::get_last_error());
#elif __linux__
  return errno;
#else
#error Not implemented
#endif
}

ra2yrcpp_exception_base::ra2yrcpp_exception_base(std::string prefix,
                                                 std::string message)
    : prefix_(prefix), message_(message) {}

const char* ra2yrcpp_exception_base::what() const throw() {
  return message_.c_str();
}

std::string ra2yrcpp::get_error_message(int error_code) {
  if (error_code == 0) {
    return std::string();
  }
#ifdef _WIN32
  return windows_utils::get_error_message(error_code);
#elif __linux__
  return strerror(error_code);
#else
#error Not Implemented
#endif
}

system_error::system_error(std::string message, int error_code) {
#if defined(_WIN32) || defined(__linux__)
  auto msg = get_error_message(error_code);
  message_ = message + " " + msg;
#else
#error Not implemented
#endif
}

system_error::system_error(std::string message)
    : system_error(message, get_last_error()) {}

const char* system_error::what() const throw() { return message_.c_str(); }
