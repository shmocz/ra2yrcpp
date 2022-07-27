#include "recordtool.hpp"

void dump_record(const std::string path) {
  bool ok = true;
  std::ifstream ii(path, std::ios_base::in | std::ios_base::binary);
  google::protobuf::io::IstreamInputStream is(&ii);
  google::protobuf::io::GzipInputStream gg(&is);
  while (ok) {
    google::protobuf::io::CodedInputStream ci(&gg);
    yrclient::ra2yr::GameState G;
    ok = yrclient::read_message(&G, &ci);
    std::cout << yrclient::to_json(G) << std::endl;
  }
}

int main(int argc, char* argv[]) {
  argparse::ArgumentParser A(argv[0]);
  A.add_argument("-i").help("input file");
  A.parse_args(argc, argv);
  dump_record(A.get<std::string>("-i"));
  return 0;
}
