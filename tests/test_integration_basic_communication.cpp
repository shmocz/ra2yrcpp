#include "protocol/protocol.hpp"

#include "config.hpp"
#include "gtest/gtest.h"
#include "integration_test.hpp"
#include "logging.hpp"
#include "manager.hpp"
#include "multi_client.hpp"
#include "utility/time.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace ra2yrcpp::manager;
using namespace std::chrono_literals;
using namespace ra2yrcpp::tests;

yrclient::game::GameSettings* default_player_settings(
    yrclient::game::GameSettings* s) {
  struct ent {
    int index;
    int ai_difficulty;
  };

  // Create players
  std::vector<ent> ee{ent{0, 0}, ent{1, 2}};
  std::for_each(ee.begin(), ee.end(), [&](ent e) {
    auto* player = s->add_players();
    player->set_index(e.index);
    player->set_ai_difficulty(e.ai_difficulty);
    (void)IntegrationTest::configure_player(
        player, fmt::format("player_{}", e.index), e.index, false);
  });

  return s;
}

auto make_unit_command(std::vector<u32> object_ids,
                       yrclient::commands::UnitAction type) {
  yrclient::commands::UnitCommand c;
  auto* a = c.mutable_args();
  a->set_action(type);
  for (auto i : object_ids) {
    a->add_object_addresses(i);
  }
  return c;
}

auto send_command(multi_client::AutoPollClient* client,
                  const google::protobuf::Message& m) {
  auto resp = client->send_command(m);
  return yrclient::from_any<yrclient::CommandResult>(resp.body());
}

template <typename T>
auto parse_response(const yrclient::Response& r) {
  auto cmd_res = yrclient::from_any<yrclient::CommandResult>(r.body());
  return yrclient::from_any<T>(cmd_res.result()).result();
}

template <typename T>
auto easy_command(multi_client::AutoPollClient* client, const T& M) {
  auto r = client->send_command(M);
  return parse_response<T>(r);
}

// get player's units
static auto get_house_objects(const yrclient::ra2yr::GameState& state,
                              const u32 id) {
  std::vector<yrclient::ra2yr::Object> res;
  auto& o = state.objects();
  std::copy_if(o.begin(), o.end(), std::back_inserter(res),
               [&](auto& o) { return o.pointer_house() == id; });
  return res;
}

TEST_F(IntegrationTest, BasicTest) {
  auto* tsett = settings();
  if (tsett == nullptr) {
    GTEST_SKIP();
  }
  yrclient::game::GameSettings sett;
  (void)default_player_settings(default_game_settings(&sett));

  Address addr(cfg::SERVER_ADDRESS, std::to_string(cfg::SERVER_PORT));

  std::vector<uint64_t> ids{0, 1};
  std::vector<int> ix;

  auto& P = sett.players();
  std::for_each(P.begin(), P.end(), [&](auto& p) {
    if (p.ai_difficulty() == 0) {
      ix.push_back(p.index());
    }
  });
  // Get non-ai settings
  std::for_each(ix.begin(), ix.end(), [&](auto i) {
    // Copy settings
    auto sett0 = sett;
    auto idir = fs::path(tsett->tmp_dir) / fmt::format("player_{}", i);
    // NB: Make instance directory beforehand
    sett0.set_player_index(i);
    manager->add_instance(new LocalGameInstance(
        GameInstance::Settings(
            sett0, "DEPRECATED",
            Address(cfg::SERVER_ADDRESS, std::to_string(cfg::SERVER_PORT))),
        idir.string()));
  });

  // Get first instance
  auto* client = manager->instances().front().get()->client();

  auto get_state = [client]() {
    return easy_command(client, yrclient::commands::GetGameState()).state();
  };

  // Create hooks and callbacks
  for (auto& s : ra2yrcppcli::INIT_COMMANDS) {
    (void)ra2yrcppcli::send_command(client, s);
  }

  auto state = get_state();
  // Wait for game to begin
  util::call_until(30000ms, 100ms, [&] {
    state = get_state();
    return !(state.stage() == yrclient::ra2yr::LoadStage::STAGE_INGAME &&
             state.current_frame() > 2);
  });

  ASSERT_EQ(state.houses().size(), sett.players().size());

  // get current player
  auto current_player = std::find_if(
      state.houses().begin(), state.houses().end(),
      [](const yrclient::ra2yr::House& h) { return h.current_player(); });

  ASSERT_NE(current_player, state.houses().end());

  auto object_types =
      easy_command(client, yrclient::commands::GetTypeClasses()).classes();

  ASSERT_GT(object_types.size(), 0);

  // map TTCs
  std::map<std::uint32_t, yrclient::ra2yr::ObjectTypeClass> ttc_map;
  for (const auto& o : object_types) {
    ttc_map[o.pointer_self()] = o;
  }
  ASSERT_GE(ttc_map.size(), 1);

  auto get_objects_by_name = [&](const auto& objects, const std::string name) {
    std::vector<yrclient::ra2yr::Object> res;
    std::copy_if(objects.begin(), objects.end(), std::back_inserter(res),
                 [&](auto& o) {
                   return ttc_map[o.pointer_technotypeclass()].name() == name;
                 });

    return res;
  };

  auto h_objs = get_house_objects(state, current_player->self());
  ASSERT_EQ(h_objs.size(), 1);

  auto objs_mcv = get_objects_by_name(h_objs, "Soviet Construction Vehicle");
  ASSERT_EQ(objs_mcv.size(), 1);
  auto obj_mcv = objs_mcv[0];

  {
    auto c = make_unit_command({obj_mcv.pointer_self()},
                               yrclient::commands::UnitAction::ACTION_SELECT);
    (void)send_command(client, c);
  }

  {
    auto c = make_unit_command({obj_mcv.pointer_self()},
                               yrclient::commands::UnitAction::ACTION_DEPLOY);
    (void)send_command(client, c);
  }

  // Wait until we have con yard
  util::call_until(5000ms, 100ms, [&] {
    h_objs = get_house_objects(get_state(), current_player->self());
    return get_objects_by_name(h_objs, "Soviet Construction Yard").empty();
  });

  // Sell
  {
    auto c = make_unit_command(
        {get_objects_by_name(h_objs, "Soviet Construction Yard")[0]
             .pointer_self()},
        yrclient::commands::UnitAction::ACTION_SELL);
    auto resp_sell = send_command(client, c);
  }

  // Wait until game exit
  util::call_until(5000ms, 100ms, [&] {
    return get_state().stage() != yrclient::ra2yr::LoadStage::STAGE_EXIT_GAME;
  });
}
