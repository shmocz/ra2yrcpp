#include "manager.hpp"

#ifdef _WIN32
#include <io.h>
#endif

using namespace ra2yrcpp::manager;

Address::Address(const std::string host, const std::string port)
    : host(host), port(port) {}

multi_client::AutoPollClient* GameInstance::client() { return client_.get(); }

GameInstance::~GameInstance() {}

std::string INISection::to_string(const std::string name,
                                  const std::vector<Entry> entries) {
  if (entries.empty()) {
    return "";
  }
  std::stringstream ss;
  ss << "[" << name << "]" << std::endl;
  std::transform(entries.begin(), entries.end(),
                 std::ostream_iterator<std::string>(ss, "\n"),
                 [](const auto& s) { return s.k + "=" + s.v; });
  return ss.str();
}

std::vector<Entry> get_other(const yrclient::game::GameSettings* s,
                             const yrclient::game::PlayerInfo* p) {
  return {
      Entry("Name", p->name()),
      Entry("Side", p->side()),
      Entry("IsSpectator", p->is_spectator()),
      Entry("Color", p->color()),
      Entry("Ip", s->tunnel_address()),
      Entry("Port", -static_cast<int>(p->index() + 2)),
  };
}

std::string GameInstance::Settings::get_spawn_ini() const {
  auto& PP = config.players();
  auto P = PP.at(config.player_index());
  auto& C = config;
  int ai_count = 0;
  std::vector<Entry> house_countries;
  std::vector<Entry> house_colors;
  std::vector<Entry> house_handicaps;  // ? AI difficulty?
  std::vector<Entry> spawn_locations;
  std::vector<Entry> tunnel_settings;
  std::vector<std::vector<Entry>> others;
  if (!config.tunnel_address().empty()) {
    tunnel_settings.push_back(Entry("Ip", config.tunnel_address()));
    tunnel_settings.push_back(Entry("Port", config.tunnel_port()));
  }

  std::for_each(PP.begin(), PP.end(), [&](auto& a) {
    a.ai_difficulty() > 0 ? ai_count++ : 0;
    const std::string k = fmt::format("Multi{}", a.index() + 1);
    if (a.index() != config.player_index()) {
      house_countries.push_back(Entry(k, a.side()));
      house_colors.push_back(Entry(k, a.color()));
      house_handicaps.push_back(Entry(k, a.ai_difficulty()));
      if (a.ai_difficulty() < 1) {
        others.push_back(get_other(&config, &a));
      }
    }
    spawn_locations.push_back(Entry(k, a.spawn_location()));
  });

  std::vector<Entry> values = {{"Name", P.name()},
                               {"Scenario", "spawnmap.ini"},
                               {"UIGameMode", C.game_mode()},
                               {"UIMapName", C.map_name()},
                               {"PlayerCount", C.players().size()},
                               {"Side", P.side()},
                               {"IsSpectator", P.is_spectator()},
                               {"Color", P.color()},
                               {"AlliesAllowed", C.allies_allowed()},
                               {"AIPlayers", ai_count},
                               {"Seed", C.random_seed()},
                               {"ShortGame", C.short_game()},
                               {"MCVRedeploy", C.mcv_redeploy()},
                               {"MultiEngineer", C.multi_engineer()},
                               {"BridgeDestroy", C.bridge_destroy()},
                               {"SuperWeapons", C.superweapons()},
                               {"BuildOffAlly", C.build_off_ally_conyards()},
                               {"Ra2Mode", C.ra2_mode()},
                               {"Credits", C.start_credits()},
                               {"UnitCount", C.unit_count()},
                               {"GameSpeed", C.game_speed()},
                               {"Bases", "Yes"},
                               {"FogOfWar", "No"},
                               {"SidebarHack", "Yes"},
                               {"AttackNeutralUnits", "Yes"},
                               {"GameMode", "1"}};

  std::stringstream ss;
  std::vector<std::string> sects = {
      INISection::to_string("Settings", values),
      INISection::to_string("HouseHandicaps", house_handicaps),
      INISection::to_string("HouseCountries", house_countries),
      INISection::to_string("HouseColors", house_colors),
      INISection::to_string("SpawnLocations", spawn_locations),
      INISection::to_string("Tunnel", tunnel_settings)};
  // Put others
  for (auto i = 0u; i < others.size(); i++) {
    sects.push_back(
        INISection::to_string(fmt::format("Other{}", i + 1), others[i]));
  }
  std::copy(sects.begin(), sects.end(),
            std::ostream_iterator<std::string>(ss, "\n"));

  return ss.str();
}

// this is just the map data
std::string GameInstance::Settings::get_spawnmap_ini() const {
  auto m = ra2yrcpp::manager::get_map_paths("INI/MPMaps.ini");
  return read_file(m[config.map_name()] + ".map");
}

GameInstance::GameInstance(const Settings settings)
    : timestamp_(
          std::chrono::high_resolution_clock::now().time_since_epoch().count()),
      settings_(settings) {}

std::uint64_t GameInstance::timestamp() const { return timestamp_; }

std::string ra2yrcpp::manager::read_file(const std::string path) {
  std::ifstream is(path);
  if (!is.good()) {
    throw std::runtime_error(fmt::format("couldnt open {}", path));
  }
  std::stringstream ss;
  ss << is.rdbuf();
  return ss.str();
}

std::map<std::string, std::string> ra2yrcpp::manager::get_map_paths(
    const std::string path_ini) {
  std::regex reg("\\[([^\\]]+)\\]\nDescription=([^\\n]+)");
  auto s = read_file(path_ini);
  std::map<std::string, std::string> res;
  auto it = std::sregex_iterator(s.begin(), s.end(), reg);
  std::sregex_iterator end;
  while (it != end) {
    if (it->size() > 2) {
      res[(*it)[2].str()] = (*it)[1].str();
    }
    it++;
  }
  return res;
}

LocalGameInstance::~LocalGameInstance() {
  // Disconnect client first
  client_ = nullptr;
  // Terminate process
}

LocalGameInstance::LocalGameInstance(const Settings sett,
                                     const std::string work_dir)
    : GameInstance(sett), work_dir_(work_dir) {
  using namespace std::chrono_literals;
  // write spawn.ini and spawnmap.ini
  char buf[1024];
  getcwd(buf, sizeof(buf));
  std::string cur_dir(buf, strchr(buf, '\0'));
  // cur_dir = out.substr(0, out.find('\0'));
  chdir(work_dir.c_str());
  {
    std::ofstream o_s("spawn.ini");
    std::ofstream o_sm("spawnmap.ini");
    dprintf("write to {}", work_dir);
    o_s << sett.get_spawn_ini();
    o_sm << sett.get_spawnmap_ini();
  }
  P_ = std::make_unique<exprocess::ExProcess>("gamemd-spawn.exe -SPAWN", "");

  // Inject DLL
  yrclient::InstrumentationService::IServiceOptions opt_I;
  opt_I.max_clients = cfg::MAX_CLIENTS;
  opt_I.port = std::stoi(sett.dest.port);
  opt_I.host = cfg::SERVER_ADDRESS;
  is_context::DLLInjectOptions opt_D;
  opt_D.delay_post = 1000u;
  opt_D.delay_pre = 2000u;
  is_context::inject_dll(P_->pid(), "libyrclient.dll", opt_I, opt_D);
  client_ = std::make_unique<multi_client::AutoPollClient>(
      opt_I.host, std::to_string(opt_I.port), 250ms, 10000ms);

  if (chdir(cur_dir.c_str())) {
    throw std::runtime_error("chdir()");
  }
}

GameInstanceManager::GameInstanceManager() {}

GameInstanceManager::~GameInstanceManager() {}

instances_t& GameInstanceManager::instances() { return instances_; }

GameInstance* GameInstanceManager::get_instance_by_timestamp(const uint64_t t) {
  auto it = std::find_if(instances_.begin(), instances_.end(),
                         [&](auto& v) { return v->timestamp() == t; });
  return it == instances_.end() ? nullptr : it->get();
}

GameInstance* GameInstanceManager::add_instance(GameInstance* I) {
  if (get_instance_by_timestamp(I->timestamp()) != nullptr) {
    throw std::runtime_error("duplicate instance");
  }
  instances().emplace_back(I);
  return I;
}
