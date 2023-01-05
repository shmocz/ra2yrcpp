#include "util_string.hpp"

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
