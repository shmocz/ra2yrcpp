#include "protocol/protocol.hpp"

#include "gtest/gtest.h"
#include "logging.hpp"
#include "ra2yrproto/ra2yr.pb.h"

#include <google/protobuf/util/message_differencer.h>

#include <cstdio>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

class TemporaryDirectoryTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  fs::path temp_dir_path_;
};

void TemporaryDirectoryTest::SetUp() {
  // create a unique path for a temporary directory
  temp_dir_path_ = fs::temp_directory_path();
  temp_dir_path_ /= std::tmpnam(nullptr);

  // create the temporary directory
  fs::create_directory(temp_dir_path_);
  iprintf("created temporary directory {}", temp_dir_path_.string());
}

void TemporaryDirectoryTest::TearDown() { fs::remove_all(temp_dir_path_); }

TEST_F(TemporaryDirectoryTest, ProtocolTest) {
  ASSERT_TRUE(fs::is_directory(temp_dir_path_));
  fs::path record_path = temp_dir_path_;
  record_path /= "record.pb.gz";
  std::vector<ra2yrproto::ra2yr::GameState> messages(32);
  std::vector<ra2yrproto::ra2yr::GameState> messages_out;
  for (std::size_t i = 0; i < messages.size(); i++) {
    auto& G = messages.at(i);
    G.set_current_frame(i + 1);
  }

  {
    auto record_out = std::make_shared<std::ofstream>(
        record_path.string(), std::ios_base::out | std::ios_base::binary);

    const bool use_gzip = true;
    yrclient::MessageOstream MS(record_out, use_gzip);

    if (std::any_of(messages.begin(), messages.end(),
                    [&MS](const auto& G) { return !MS.write(G); })) {
      throw std::runtime_error("failed to write message to output stream");
    }
  }

  ASSERT_TRUE(fs::is_regular_file(record_path));
  yrclient::dump_messages(record_path.string(), ra2yrproto::ra2yr::GameState(),
                          [&messages_out](auto* M) {
                            ra2yrproto::ra2yr::GameState G;
                            G.CopyFrom(*M);
                            messages_out.push_back(G);
                          });

  google::protobuf::util::MessageDifferencer D;
  for (std::size_t i = 0; i < messages.size(); i++) {
    ASSERT_TRUE(D.Equals(messages.at(i), messages_out.at(i)));
  }
}
