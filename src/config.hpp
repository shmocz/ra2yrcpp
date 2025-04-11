#pragma once
#include "constants.hpp"

#include <string>

namespace ra2yrcpp {
namespace config {

struct ConfigData {
  bool debug_log{false};
  std::string record_filename;
  std::string traffic_filename;
  unsigned parse_map_data_interval{1U};
  bool single_step{false};
  unsigned port{cfg::SERVER_PORT};
  unsigned max_connections{cfg::MAX_CLIENTS};
  std::string allowed_hosts_regex{cfg::ALLOWED_HOSTS_REGEX};
  std::string log_filename;
  ConfigData() = delete;

  /// Parse configuration from JSON string.
  ///
  /// @param json
  /// @exception std::runtime_error if parsing fails
  static ConfigData parse(std::string json);
};

class Config {
 public:
  explicit Config(ConfigData c);
  explicit Config(std::string json);
  [[nodiscard]] const ConfigData& c() const;
  void set_debug_log(bool value);
  void set_allowed_hosts_regex(std::string pattern);
  void set_max_connections(unsigned value);
  void set_parse_map_data_interval(unsigned value);
  void set_single_step(bool value);
  Config() = delete;
  Config(const Config&) = delete;
  Config& operator=(const Config&) = delete;
  std::string to_json();

 private:
  ConfigData c_;
};

}  // namespace config
}  // namespace ra2yrcpp
