#include "util_string.hpp"

#include <regex>
#include <string>
#include <vector>

std::vector<std::string> ra2yrcpp::split_string(const std::string& str,
                                                std::string delim) {
  std::vector<std::string> tokens;
  std::regex re(delim);
  std::sregex_token_iterator first{str.begin(), str.end(), re, -1}, last;
  for (; first != last; ++first) {
    tokens.push_back(*first);
  }
  return tokens;
}
