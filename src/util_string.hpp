#pragma once
#include "types.h"

#include <iomanip>
#include <regex>
#include <string>
#include <vector>

namespace yrclient {
inline vecu8 to_bytes(std::string msg) { return vecu8(msg.begin(), msg.end()); }

inline std::string to_string(const vecu8 bytes) {
  return std::string(bytes.begin(), bytes.end());
}

template <typename T>
inline std::string to_hex(const T i) {
  std::stringstream ss;
  ss << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << i;
  return ss.str();
}

// split string by delimiter regex and return vector of strings
std::vector<std::string> split_string(const std::string& s,
                                      const std::string delim = "[\\s]+");

// join vector of strings
std::string join_string(const std::vector<std::string> v,
                        const std::string delim = " ");

// parse hex number from string
u32 parse_address(const std::string s);
}  // namespace yrclient
