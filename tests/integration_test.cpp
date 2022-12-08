#include "integration_test.hpp"

#include "process.hpp"

#include <filesystem>

using namespace ra2yrcpp::tests;

void IntegrationTest::init() {}

void IntegrationTest::deinit() {}

void IntegrationTest::TearDown() {
  manager = nullptr;
  deinit();
}

Settings* IntegrationTest::settings() {
  auto& s = settings_;
  std::vector<std::pair<std::string*, std::string>> d = {
      {&s.game_dir, "RA2YRCPP_GAME_DIR"},
      {&s.tmp_dir, "RA2YRCPP_TEST_INSTANCES_DIR"},
      {&s.tunnel_url, "RA2YRCPP_TUNNEL_URL"}};
  for (auto v : d) {
    char* e = getenv(v.second.c_str());
    if (e == nullptr) {
      return nullptr;
    }
    *v.first = std::string(e);
  }
  return &settings_;
}

void IntegrationTest::SetUp() {
  network::Init();
  manager = std::make_unique<std::remove_reference_t<decltype(*manager)>>();
  init();
}

ra2yrproto::game::PlayerInfo* IntegrationTest::configure_player(
    ra2yrproto::game::PlayerInfo* s, const std::string name,
    const uint32_t spawn_location, const bool is_spectator) {
  s->set_name(name);
  s->set_spawn_location(spawn_location);
  s->set_is_spectator(is_spectator);
  s->set_color(spawn_location + 1);
  s->set_side(6);
  return s;
}

ra2yrproto::game::GameSettings* IntegrationTest::default_game_settings(
    ra2yrproto::game::GameSettings* s) {
  s->set_game_speed(1u);
  s->set_map_name("[4] Dry Heat");
  s->set_game_mode("Battle");
  s->set_mcv_redeploy(true);
  s->set_start_credits(10000u);
  s->set_unit_count(0u);
  s->set_random_seed(1852262696u);
  s->set_short_game(true);
  return s;
}

void IntegrationTest::make_instance_directory(const fs::path game_dir,
                                              const fs::path tmp_dir) {
  // Make directory
  if (!fs::exists(tmp_dir)) {
    fs::create_directories(tmp_dir);
  }

  // Symlink static data files
  const std::vector<std::string> paths = {"Killer.mmx"};
  for (const auto& p : paths) {
    fs::create_symlink(game_dir / p, tmp_dir / p);
  }
}
