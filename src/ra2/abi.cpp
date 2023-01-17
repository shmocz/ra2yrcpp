#include "ra2/abi.hpp"

#include <xbyak/xbyak.h>

using namespace ra2::abi;

struct ThisCall : Xbyak::CodeGenerator {
  explicit ThisCall(const u32 p_fn, const size_t stack_size = 0u) {
    mov(ecx, ptr[esp + 0x4]);  // this

    // copy stack after this (push args)
    for (auto i = 0u; i * 4u < stack_size; i++) {
      mov(eax, ptr[esp + 0x4 + (stack_size - i * 4u)]);
      mov(ptr[esp - (i + 1) * 4u], eax);
    }
    sub(esp, stack_size);
    mov(eax, p_fn);  // target function
    call(eax);       // callee cleans the stack for __thiscall
    ret();           // stack size is maintained, we are safe to return
  }
};

// Call a varargs function such as printf by creating a empty area in the stack
// and copying original arguments. NB. possibly unsafe.
struct FastCall : Xbyak::CodeGenerator {
  explicit FastCall(const u32 p_fn) {  // fn(args_begin, count)
    // args_begin
    mov(esi, ptr[esp + 0x4]);
    // number of bytes to copy from args_begin
    mov(ecx, ptr[esp + 0x8]);
    push(ebp);
    mov(ebp, esp);
    sub(esp, ecx);
    mov(edi, esp);
    rep();
    movsb();
    mov(eax, p_fn);
    call(eax);  // cdecl - calling function cleans the stack
    mov(esp, ebp);
    pop(ebp);  // restore stack
    ret();
  }
};

template <std::uintptr_t address, typename CallT, typename GenT = ThisCall,
          bool IsImport = false>
struct Caller {
  using Traits = utility::FunctionTraits<CallT>;
  using gen_t = GenT;
  static constexpr auto stack_size = (Traits::NumArgs - 1) * 4u;
  static constexpr auto ptr = address;

  static auto addr() {
    return IsImport ? serialize::read_obj_le<std::uintptr_t>(address) : address;
  }

  template <typename... Args>
  static auto call(const ra2::abi::ABIGameMD& A, Args... args) {
    return reinterpret_cast<GenT*>(A.code_generators().at(addr()).get())
        ->template getCode<CallT>()(args...);
  }
};

// xbyak windows includes pollute namespace (clash with SelectObject)
namespace ra2 {
using ClickMission =
    Caller<0x6FFBE0U,
           void __cdecl (*)(std::uintptr_t object, ra2::general::Mission m,
                            std::uintptr_t target_object,
                            ra2::game_screen::CellClass* target_cell,
                            CellClass* nearest_target_cell)>;

using ClickEvent = Caller<0x6FFE00U, bool __cdecl (*)(u32, u8)>;

using DeployObject = Caller<0x7393C0U, void __cdecl (*)(u32)>;

using SellBuilding = Caller<0x447110U, bool __cdecl (*)(u32, u32)>;

using SelectObject = Caller<0x6FBFA0u, bool __cdecl (*)(u32)>;

using BuildingClass_CanPlaceHere =
    Caller<0x464AC0U, bool __cdecl (*)(std::uintptr_t, vectors::CellStruct*,
                                       std::uintptr_t)>;

using AddMessage =
    Caller<0x5D3BA0U, bool __cdecl (*)(const std::uintptr_t, const wchar_t*,
                                       int, const wchar_t*, const i32,
                                       const i32, const u32, bool)>;

using sprintf =
    Caller<0x7E14B0U, void __cdecl (*)(const char*, const std::uintptr_t),
           FastCall, true>;

};  // namespace ra2

ABIGameMD::ABIGameMD() {
  add_entry<ra2::SelectObject>();
  add_entry<ra2::DeployObject>();
  add_entry<ra2::SellBuilding>();
  add_entry<ra2::ClickEvent>();
  add_entry<ra2::ClickMission>();
  add_entry<ra2::BuildingClass_CanPlaceHere>();
  add_entry<ra2::AddMessage>();
  add_entry<FastCall>(
      serialize::read_obj_le<std::uintptr_t>(ra2::sprintf::ptr));
}

const std::map<u32, std::unique_ptr<void, ABIGameMD::deleter_t>>&
ABIGameMD::code_generators() const {
  return code_generators_;
}

bool ABIGameMD::SelectObject(const u32 address) const {
  return ra2::SelectObject::call(*this, address);
}

void ABIGameMD::SellBuilding(const u32 address) const {
  ra2::SellBuilding::call(*this, address, 1);
}

void ABIGameMD::DeployObject(const u32 address) const {
  ra2::DeployObject::call(*this, address);
}

bool ABIGameMD::ClickEvent(const u32 address, const u8 event) const {
  return ra2::ClickEvent::call(*this, address, event);
}

void ABIGameMD::sprintf(char** buf, const std::uintptr_t args_start) const {
  char fake_stack[128];
  auto val = utility::asint(buf);
  std::memset(&fake_stack[0], 0, sizeof(fake_stack));
  std::memcpy(&fake_stack[0], &val, 4U);
  // copy rest of the args (WARNING: out of bounds read)
  std::memcpy(&fake_stack[4], utility::asptr<char*>(args_start),
              sizeof(fake_stack) - 4U);
  ra2::sprintf::call(*this, &fake_stack[0], sizeof(fake_stack));
}

void ABIGameMD::ClickedMission(std::uintptr_t object, ra2::general::Mission m,
                               std::uintptr_t target_object,
                               ra2::game_screen::CellClass* target_cell,
                               CellClass* nearest_target_cell) const {
  ClickMission::call(*this, object, m, target_object, target_cell,
                     nearest_target_cell);
}

u32 ABIGameMD::timeGetTime() {
  return reinterpret_cast<u32 __stdcall (*)()>(
      serialize::read_obj_le<std::uintptr_t>(0x7E1530))();
}

bool ABIGameMD::BuildingClass_CanPlaceHere(std::uintptr_t p_this,
                                           vectors::CellStruct* cell,
                                           std::uintptr_t house_owner) const {
  return ra2::BuildingClass_CanPlaceHere::call(*this, p_this, cell,
                                               house_owner);
}

void ABIGameMD::AddMessage(int id, const std::string message, const i32 color,
                           const i32 style, const u32 duration_frames,
                           bool single_player) {
  static constexpr std::uintptr_t MessageListClass = 0xA8BC60U;
  std::wstring m(message.begin(), message.end());
  ra2::AddMessage::call(*this, MessageListClass, nullptr, id, m.c_str(), color,
                        style, duration_frames, single_player);
}
