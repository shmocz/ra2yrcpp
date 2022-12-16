#include "auto_thread.hpp"

using namespace utility;

auto_thread::auto_thread(std::function<void(void)> fn)
    : _thread(std::thread(fn)) {}

auto_thread::~auto_thread() { _thread.join(); }
