#include "command/command_manager.hpp"
#include "gtest/gtest.h"
#include "utility.h"
#include "utility/time.hpp"

#include <chrono>
#include <memory>
#include <vector>

using namespace std::chrono_literals;

class CommandManagerTest : public ::testing::Test {
 protected:
  command::CommandManager M;
};

class GenerateReadableString {
  static constexpr char syms[] = "abcdefghijklmnopqrstuvxyz";

 public:
  explicit GenerateReadableString(const std::size_t length) : msg_(length, 0) {}

  void operator()(const std::vector<unsigned int> sequence) {
    for (auto i = 0u; i < msg_.size(); i++) {
      msg_[i] = syms[sequence[i % sequence.size()]];
    }
  }

  auto& msg() { return msg_; }

 private:
  std::string msg_;
};

TEST_F(CommandManagerTest, DummyCommand) {
  GenerateReadableString G(32);
  G({1, 2, 3, 4, 5});
}

TEST_F(CommandManagerTest, RegisterAndRunCommandWithResources) {
  std::string flag = "";
  const std::string key = "key";
  const std::string cmd_name = "test_cmd";
  std::uint64_t queue = 0u;
  const size_t count = 100;
  M.factory().add_entry(cmd_name, [&flag](command::Command* c) {
    auto* G = reinterpret_cast<GenerateReadableString*>(c->args());
    (*G)({1, 2, 3, 4, 5});
    flag.assign(G->msg());
  });

  M.create_queue(queue);
  for (size_t i = 0u; i < count; i++) {
    auto* G = new GenerateReadableString(1024);
    auto cmd = std::shared_ptr<command::Command>(M.factory().make_command(
        cmd_name,
        std::unique_ptr<void, void (*)(void*)>(
            G,
            [](auto d) {
              delete reinterpret_cast<GenerateReadableString*>(d);
            }),
        queue));
    M.enqueue_command(cmd);
  }
  std::vector<command::cmd_entry_t> tot;
  util::call_until(5000ms, 10ms, [&]() {
    auto r = M.flush_results(queue);
    if (!r.empty()) {
      tot.insert(tot.end(), r.begin(), r.end());
    }
    return tot.size() < count;
  });
  ASSERT_EQ(tot.size(), count);
}

TEST_F(CommandManagerTest, RegisterAndRunCommand) {
  std::string flag = "";
  const std::string key = "key";
  const std::string cmd_name = "test_cmd";
  std::uint64_t queue = 0u;
  M.factory().add_entry(cmd_name, [&key](command::Command* c) {
    reinterpret_cast<decltype(flag)*>(c->args())->assign(key);
  });
  auto cmd = std::shared_ptr<command::Command>(
      M.factory().make_command(cmd_name, &flag, queue));
  M.create_queue(queue);
  M.enqueue_command(cmd);
  std::vector<command::cmd_entry_t> r;
  util::call_until(5000ms, 100ms, [&]() {
    r = M.flush_results(queue);
    return r.size() < 1;
  });
  ASSERT_EQ(r.size(), 1);
  ASSERT_EQ(flag, key);
}
