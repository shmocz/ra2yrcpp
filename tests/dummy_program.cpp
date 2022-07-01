#include "utility/time.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " count delay" << std::endl;
    return 1;
  }

  auto count = std::stoul(argv[1]);
  auto delay = std::stoul(argv[2]);
  for (unsigned long i = 0; i < count; i++) {
    std::cout << i << "/" << count << std::endl;
    util::sleep_ms(delay);
  }
  return 0;
}
