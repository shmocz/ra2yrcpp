#pragma once
#include "exprocess.hpp"
#include "logging.hpp"
#include "multi_client.hpp"
#include "ra2yrcppcli/ra2yrcppcli.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ra2yrcpp {
namespace manager {

// TODO: this is idiotic.. get a proper INI parser
struct Entry {
  std::string k;
  std::string v;

  Entry(const std::string k, const char* v) : k(k), v(v) {}
  Entry(const std::string k, const std::string v) : k(k), v(v) {}
  Entry(const std::string k, const bool v) : k(k), v(v ? "True" : "False") {}
  template <typename T>
  Entry(const std::string k, T v) : k(k), v(std::to_string(v)) {}
};

struct INISection {
  static std::string to_string(const std::string name,
                               const std::vector<Entry> entries);
};

struct GameConfig {
  yrclient::game::GameSettings s;

  GameConfig();
  std::string to_ini();
};

struct Address {
  std::string host;
  std::string port;
  Address(const std::string host, const std::string port);
};

class GameInstance {
 public:
  struct Settings {
    yrclient::game::GameSettings config;
    const std::string map_name;
    Address dest;

    Settings(const yrclient::game::GameSettings config,
             const std::string map_name, const Address dest)
        : config(config), map_name(map_name), dest(dest) {}

    std::string get_spawn_ini() const;

    std::string get_spawnmap_ini() const;
  };

  explicit GameInstance(const Settings settings_);

  multi_client::AutoPollClient* client();

  // Game instance creation time
  std::uint64_t timestamp() const;

  virtual ~GameInstance();

 protected:
  const std::uint64_t timestamp_;
  const Settings settings_;
  std::unique_ptr<multi_client::AutoPollClient> client_;
};

class LocalGameInstance : public ra2yrcpp::manager::GameInstance {
  std::unique_ptr<exprocess::ExProcess> P_;
  const std::string work_dir_;

 public:
  explicit LocalGameInstance(const Settings sett,
                             const std::string work_dir = "");
  ~LocalGameInstance() override;
};

std::map<std::string, std::string> get_map_paths(const std::string path_ini);

std::string read_file(const std::string path);

using instances_t = std::vector<std::unique_ptr<GameInstance>>;

class GameInstanceManager {
 public:
  GameInstanceManager();
  ~GameInstanceManager();
  GameInstance* add_instance(GameInstance* I);
  instances_t& instances();
  GameInstance* get_instance_by_timestamp(const uint64_t t);

 private:
  instances_t instances_;
};

}  // namespace manager
}  // namespace ra2yrcpp
