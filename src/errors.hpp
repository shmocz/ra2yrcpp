#pragma once
#include <exception>
#include <string>

namespace ra2yrcpp {

int get_last_error();
std::string get_error_message(const int error_code);

class not_implemented : public std::exception {
 public:
  explicit not_implemented(const std::string message = "");
  virtual const char* what() const throw();

 private:
  std::string message_;
};

class ra2yrcpp_exception_base : public std::exception {
 public:
  explicit ra2yrcpp_exception_base(const std::string prefix,
                                   const std::string message);
  virtual const char* what() const throw();

 protected:
  std::string prefix_;
  std::string message_;
};

class general_error : public ra2yrcpp_exception_base {
 public:
  explicit general_error(const std::string message);
};

class system_error : public std::exception {
 public:
  system_error(const std::string message, const int error_code);
  explicit system_error(const std::string message);
  virtual const char* what() const throw();

 private:
  std::string message_;
};

class timeout : public std::exception {
 public:
  explicit timeout(const std::string message = "");
  virtual const char* what() const throw();

 private:
  std::string message_;
};

class protocol_error : public ra2yrcpp_exception_base {
 public:
  explicit protocol_error(const std::string message = "");
};

}  // namespace ra2yrcpp
