#include "ra2/abi.hpp"

#include <xbyak/xbyak.h>

using namespace ra2::abi;

struct VirtualCall : Xbyak::CodeGenerator {
  explicit VirtualCall(const u32 vtable_index, const size_t stack_size = 0u) {
    mov(ecx, ptr[esp + 0x4]);  // this

    // copy stack after this (push args)
    for (auto i = 0u; i * 4u < stack_size; i++) {
      mov(eax, ptr[esp + 0x4 + (stack_size - i * 4u)]);
      mov(ptr[esp - (i + 1) * 4u], eax);
    }
    sub(esp, stack_size);
    mov(eax, ptr[ecx]);  // vtable addres
    mov(eax, ptr[eax + (vtable_index * 0x4)]);
    call(eax);  // callee cleans the stack for __thiscall
    ret();      // stack size is maintained, we are safe to return
  }
};

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
  using FirstArg = typename std::tuple_element<0, typename Traits::ArgsT>::type;
  static constexpr bool IsVirtual = std::is_same_v<GenT, VirtualCall>;
  static constexpr auto stack_size = (Traits::NumArgs - 1) * 4u;
  static constexpr auto ptr = address;

  static auto addr() {
    return IsImport ? serialize::read_obj_le<std::uintptr_t>(address) : address;
  }

  static auto vaddr(FirstArg object) {
    return serialize::read_obj<std::uintptr_t>(
        serialize::read_obj<std::uintptr_t>(object) + 0x4 * addr());
  }

  template <typename... Args>
  static auto call_virtual(ra2::abi::ABIGameMD* A, FirstArg object,
                           Args... args) {
    auto p_virtual_function = vaddr(object);
    auto& C = A->code_generators();
    if (C.find(p_virtual_function) == C.end()) {
      A->add_virtual<VirtualCall>(addr(), p_virtual_function);
    }
    return reinterpret_cast<GenT*>(C.at(p_virtual_function).get())
        ->template getCode<CallT>()(object, args...);
  }

  // TODO: jit other trampolines on demand
  template <typename... Args>
  static auto call(ra2::abi::ABIGameMD* A, Args... args) {
    if constexpr (IsVirtual) {
      return call_virtual(A, args...);
    } else {
      return reinterpret_cast<GenT*>(A->code_generators().at(addr()).get())
          ->template getCode<CallT>()(args...);
    }
  }
};

// xbyak windows includes pollute namespace (clash with SelectObject)
namespace ra2 {
using ClickMission =
    Caller<0x6FFBE0U, void __cdecl (*)(std::uintptr_t object, Mission m,
                                       std::uintptr_t target_object,
                                       CellClass* target_cell,
                                       CellClass* nearest_target_cell)>;

using ClickEvent = Caller<0x6FFE00U, bool __cdecl (*)(u32, u8)>;

using DeployObject = Caller<0x7393C0U, void __cdecl (*)(u32)>;

using SellBuilding = Caller<0x447110U, bool __cdecl (*)(u32, u32)>;

using SelectObject = Caller<0x6FBFA0u, bool __cdecl (*)(u32)>;

using BuildingClass_CanPlaceHere =
    Caller<0x464AC0U,
           bool __cdecl (*)(std::uintptr_t, CellStruct*, std::uintptr_t)>;

using DisplayClass_Passes_Proximity_Check =
    Caller<0x4A8EB0U,
           bool __cdecl (*)(  // NOLINT
               std::uintptr_t p_this, BuildingTypeClass* p_object,
               u32 house_index, CellStruct* foundation, CellStruct* cell)>;

using DisplayClass_SomeCheck =
    Caller<0x4A9070,
           bool __cdecl (*)(  // NOLINT
               std::uintptr_t p_this, BuildingTypeClass* p_object,
               u32 house_index, CellStruct* foundation, CellStruct* cell)>;

// TODO: dynamic dispatch
using BuildingTypeClass_GetFoundationData =
    Caller<0x45EC20U, CellStruct* __cdecl (*)(  // NOLINT
                          BuildingTypeClass* p_this, bool includeBib)>;

using AddMessage =
    Caller<0x5D3BA0U, bool __cdecl (*)(const std::uintptr_t, const wchar_t*,
                                       int, const wchar_t*, const i32,
                                       const i32, const u32, bool)>;

using sprintf =
    Caller<0x7E14B0U, void __cdecl (*)(const char*, const std::uintptr_t),
           FastCall, true>;

using CellClass_GetContainedTiberiumValue =
    Caller<0x485020U, int __cdecl (*)(std::uintptr_t)>;

using AbstractClass_WhatAmI =
    Caller<11, AbstractType __cdecl (*)(AbstractClass*), VirtualCall>;

};  // namespace ra2

ABIGameMD::ABIGameMD() {
  add_entry<ra2::SelectObject>();
  add_entry<ra2::DeployObject>();
  add_entry<ra2::SellBuilding>();
  add_entry<ra2::ClickEvent>();
  add_entry<ra2::ClickMission>();
  add_entry<ra2::BuildingClass_CanPlaceHere>();
  add_entry<ra2::AddMessage>();
  add_entry<ra2::CellClass_GetContainedTiberiumValue>();
  add_entry<ra2::DisplayClass_Passes_Proximity_Check>();
  add_entry<ra2::DisplayClass_SomeCheck>();
  add_entry<ra2::BuildingTypeClass_GetFoundationData>();
  add_entry<FastCall>(
      serialize::read_obj_le<std::uintptr_t>(ra2::sprintf::ptr));
}

std::map<u32, std::unique_ptr<void, ABIGameMD::deleter_t>>&
ABIGameMD::code_generators() {
  return code_generators_;
}

bool ABIGameMD::SelectObject(const u32 address) {
  return ra2::SelectObject::call(this, address);
}

void ABIGameMD::SellBuilding(const u32 address) {
  ra2::SellBuilding::call(this, address, 1);
}

void ABIGameMD::DeployObject(const u32 address) {
  ra2::DeployObject::call(this, address);
}

bool ABIGameMD::ClickEvent(const u32 address, const u8 event) {
  return ra2::ClickEvent::call(this, address, event);
}

void ABIGameMD::sprintf(char** buf, const std::uintptr_t args_start) {
  char fake_stack[128];
  auto val = ::utility::asint(buf);
  std::memset(&fake_stack[0], 0, sizeof(fake_stack));
  std::memcpy(&fake_stack[0], &val, 4U);
  // copy rest of the args (WARNING: out of bounds read)
  std::memcpy(&fake_stack[4], ::utility::asptr<char*>(args_start),
              sizeof(fake_stack) - 4U);
  ra2::sprintf::call(this, &fake_stack[0], sizeof(fake_stack));
}

void ABIGameMD::ClickedMission(std::uintptr_t object, Mission m,
                               std::uintptr_t target_object,
                               CellClass* target_cell,
                               CellClass* nearest_target_cell) {
  ClickMission::call(this, object, m, target_object, target_cell,
                     nearest_target_cell);
}

u32 ABIGameMD::timeGetTime() {
  return reinterpret_cast<u32 __stdcall (*)()>(
      serialize::read_obj_le<std::uintptr_t>(0x7E1530))();
}

int ABIGameMD::CellClass_GetContainedTiberiumValue(std::uintptr_t p_this) {
  return CellClass_GetContainedTiberiumValue::call(this, p_this);
}

bool ABIGameMD::BuildingClass_CanPlaceHere(std::uintptr_t p_this,
                                           CellStruct* cell,
                                           std::uintptr_t house_owner) {
  return BuildingClass_CanPlaceHere::call(this, p_this, cell, house_owner);
}

bool ABIGameMD::DisplayClass_Passes_Proximity_Check(std::uintptr_t p_this,
                                                    BuildingTypeClass* p_object,
                                                    u32 house_index,
                                                    CellStruct* cell) {
  auto* fnd = BuildingTypeClass_GetFoundationData::call(this, p_object, true);

  return ra2::DisplayClass_Passes_Proximity_Check::call(
             this, p_this, p_object, house_index, fnd, cell) &&
         ra2::DisplayClass_SomeCheck::call(this, p_this, p_object, house_index,
                                           fnd, cell);
}

void ABIGameMD::AddMessage(int id, const std::string message, const i32 color,
                           const i32 style, const u32 duration_frames,
                           bool single_player) {
  static constexpr std::uintptr_t MessageListClass = 0xA8BC60U;
  std::wstring m(message.begin(), message.end());
  ra2::AddMessage::call(this, MessageListClass, nullptr, id, m.c_str(), color,
                        style, duration_frames, single_player);
}

AbstractType ABIGameMD::AbstractClass_WhatAmI(AbstractClass* object) {
  return ra2::AbstractClass_WhatAmI::call(this, object);
}
