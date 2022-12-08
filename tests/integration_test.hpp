#pragma once
#include "gtest/gtest.h"
#include "manager.hpp"
#include "network.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ra2yrcpp {
namespace tests {

namespace fs = std::filesystem;

struct Settings {
  std::string game_dir;
  std::string tmp_dir;
  std::string tunnel_url;
};

class IntegrationTest : public ::testing::Test {
 public:
  static ra2yrproto::game::PlayerInfo* configure_player(
      ra2yrproto::game::PlayerInfo* s, const std::string name,
      const uint32_t spawn_location, const bool is_spectator);

  static ra2yrproto::game::GameSettings* default_game_settings(
      ra2yrproto::game::GameSettings* s);

  void make_instance_directory(const fs::path game_dir, const fs::path tmp_dir);

 protected:
  void SetUp() override;

  virtual void init();
  virtual void deinit();

  void TearDown() override;
  Settings* settings();

  Settings settings_;
  std::unique_ptr<ra2yrcpp::manager::GameInstanceManager> manager;
};

}  // namespace tests
}  // namespace ra2yrcpp
