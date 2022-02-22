#pragma once
#include <string>

namespace yrclient {

int get_last_error();
std::string get_error_message(const int error_code);
class not_implemented : public std::exception {
 public:
  not_implemented(const std::string message = "");
  const char* what() const throw();

 private:
  std::string message_;
};

class system_error : public std::exception {
 public:
  system_error(const std::string message, const int error_code);
  system_error(const std::string message);
  const char* what() const throw();

 private:
  std::string message_;
};

class timeout : public std::exception {
 public:
  timeout(const std::string message = "");
  const char* what() const throw();

 private:
  std::string message_;
};

}  // namespace yrclient