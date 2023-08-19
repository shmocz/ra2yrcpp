#include "errors.hpp"

#ifdef _WIN32
#include "win32/win_message.hpp"
#elif __linux__
#include <cstring>

#include <cerrno>
#endif

using namespace yrclient;

int yrclient::get_last_error() {
#ifdef _WIN32
  return static_cast<int>(windows_utils::get_last_error());
#elif __linux__
  return errno;
#else
#error Not implemented
#endif
}

ra2yrcpp_exception_base::ra2yrcpp_exception_base(const std::string prefix,
                                                 const std::string message)
    : prefix_(prefix), message_(message) {}

const char* ra2yrcpp_exception_base::what() const throw() {
  return message_.c_str();
}

general_error::general_error(const std::string message)
    : ra2yrcpp_exception_base("General error", message) {
  message_ = prefix_ + ": " + message_;
}

std::string yrclient::get_error_message(const int error_code) {
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

not_implemented::not_implemented(const std::string message)
    : message_(message) {}

const char* not_implemented::what() const throw() { return message_.c_str(); }

system_error::system_error(const std::string message, const int error_code) {
#if defined(_WIN32) || defined(__linux__)
  auto msg = get_error_message(error_code);
  message_ = message + " " + msg;
#else
#error Not implemented
#endif
}

system_error::system_error(const std::string message)
    : system_error(message, get_last_error()) {}

const char* system_error::what() const throw() { return message_.c_str(); }

timeout::timeout(const std::string message) : message_(message) {}

const char* timeout::what() const throw() { return message_.c_str(); }

protocol_error::protocol_error(const std::string message)
    : ra2yrcpp_exception_base("Protocol error", message) {}
