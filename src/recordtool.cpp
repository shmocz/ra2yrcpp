#include "recordtool.hpp"

template <typename MessageT>
void dump_messages(const std::string path) {
  bool ok = true;
  std::ifstream ii(path, std::ios_base::in | std::ios_base::binary);
  google::protobuf::io::IstreamInputStream is(&ii);
  google::protobuf::io::GzipInputStream gg(&is);
  while (ok) {
    google::protobuf::io::CodedInputStream ci(&gg);
    MessageT G;
    ok = yrclient::read_message(&G, &ci);
    fmt::print("{}\n", yrclient::to_json(G));
  }
}

int main(int argc, char* argv[]) {
  argparse::ArgumentParser A(argv[0]);
  A.add_argument("--mode").default_value("record").help("input file");
  A.add_argument("-i").help("input file");
  A.parse_args(argc, argv);
  const std::string input = A.get<std::string>("-i");
  auto mode = A.get<std::string>("mode");
  if (mode == "record") {
    dump_messages<ra2yrproto::ra2yr::GameState>(input);
  } else if (mode == "traffic") {
    dump_messages<ra2yrproto::ra2yr::TunnelPacket>(input);
  }
  return 0;
}
