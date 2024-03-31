#pragma once
#include <exception>
#include <string>

namespace ra2yrcpp {

int get_last_error();
std::string get_error_message(int error_code);

class not_implemented : public std::exception {
 public:
  explicit not_implemented(std::string message = "");
  virtual const char* what() const throw();

 private:
  std::string message_;
};

class ra2yrcpp_exception_base : public std::exception {
 public:
  explicit ra2yrcpp_exception_base(std::string prefix, std::string message);
  virtual const char* what() const throw();

 protected:
  std::string prefix_;
  std::string message_;
};

class general_error : public ra2yrcpp_exception_base {
 public:
  explicit general_error(std::string message);
};

class system_error : public std::exception {
 public:
  system_error(std::string message, int error_code);
  explicit system_error(std::string message);
  virtual const char* what() const throw();

 private:
  std::string message_;
};

class timeout : public std::exception {
 public:
  explicit timeout(std::string message = "");
  virtual const char* what() const throw();

 private:
  std::string message_;
};

class protocol_error : public ra2yrcpp_exception_base {
 public:
  explicit protocol_error(std::string message = "");
};

}  // namespace ra2yrcpp
