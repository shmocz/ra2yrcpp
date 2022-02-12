#include "hook.hpp"
#include <iostream>

using namespace hook;

Hook::Hook(const addr_t src_address, const size_t code_length)
    : d_{src_address, 0u, code_length}, dm_(this) {
  // Create detour
  auto p = dm_.getCode<u8*>();
  // Copy original instruction
  memcpy(p, reinterpret_cast<void*>(src_address), code_length);

  // Suspend & resume until none of the threads are at target region

  // Patch target region
  install_jump(src_address, p, code_length);
}

Hook::~Hook() {
  // Remove all callbacks

  // Suspend & resume until no thread is at target region

  // Patch back original code

  // If no thread is executing the hook, free detour trampoline directly

  // Otherwise enable cleanup handler in the hook to be run by last thread
  // exiting the hook

  // Resume threads (potentially waiting for cleanup to complete)
}

void Hook::add_callback(UserCallback c) { callbacks_.push_back(c); }
void Hook::call(Hook* H, X86Regs state) {
  H->lock();
  for (auto& c : H->callbacks()) {
    c.func(H, c.args, state);
  }
  H->unlock();
}

std::vector<UserCallback>& Hook::callbacks() { return callbacks_; }
void Hook::lock() {}
void Hook::unlock() {}
Detour& Hook::detour() { return d_; }
u8* Hook::codebuf() { return codebuf_; }

void Hook::install_jump(const addr_t target, const u8* detour,
                        const size_t code_length) {
  DetourTrampoline D(detour, code_length);
  auto f = D.getCode<void*>();
  memcpy(reinterpret_cast<u8*>(target), f, D.getSize());
}