#include "util_string.hpp"
#include <regex>

std::vector<std::string> yrclient::split_string(const std::string& str,
                                                const std::string delim) {
  std::vector<std::string> tokens;
  std::regex re(delim);
  std::sregex_token_iterator first{str.begin(), str.end(), re, -1}, last;
  for (; first != last; ++first) {
    tokens.push_back(*first);
  }
  return tokens;
}

std::string yrclient::join_string(const std::vector<std::string> v,
                                  const std::string delim) {
  if (v.empty()) {
    return "";
  }
  std::stringstream ss;
  for (size_t i = 1; i < v.size(); i++) {
    ss << v[i - 1] << delim;
  }
  ss << v.back();
  return ss.str();
}

u32 yrclient::parse_address(const std::string s) {
  std::regex re("0x[0-9a-fA-F]+");
  if (!std::regex_match(s, re)) {
    throw std::runtime_error("invalid address");
  }
  return std::stoul(s, nullptr, 16);
}
