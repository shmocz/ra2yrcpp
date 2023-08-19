#pragma once
#include <string>

namespace windows_utils {

std::string get_error_message(const int error_code);
unsigned long get_last_error();
}  // namespace windows_utils
