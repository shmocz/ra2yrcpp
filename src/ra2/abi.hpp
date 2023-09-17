#pragma once

#include "ra2/yrpp_export.hpp"
#include "types.h"
#include "utility/array_iterator.hpp"
#include "utility/function_traits.hpp"
#include "utility/serialize.hpp"
#include "utility/sync.hpp"

#include <xbyak/xbyak.h>

#include <cstddef>
#include <cstdint>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace ra2 {
namespace abi {

using codegen_store = std::map<u32, std::unique_ptr<Xbyak::CodeGenerator>>;

class ABIGameMD {
 public:
  // JIT the functions here
  ABIGameMD();

  bool SelectObject(u32 address);

  void SellBuilding(u32 address);

  void DeployObject(u32 address);

  bool ClickEvent(u32 address, u8 event);

  void sprintf(char** buf, std::uintptr_t args_start);

  bool BuildingClass_CanPlaceHere(std::uintptr_t p_this, CellStruct* cell,
                                  std::uintptr_t house_owner);

  void AddMessage(int id, const std::string message, i32 color, i32 style,
                  u32 duration_frames, bool single_player);

  u32 timeGetTime();

  bool DisplayClass_Passes_Proximity_Check(std::uintptr_t p_this,
                                           BuildingTypeClass* p_object,
                                           u32 house_index, CellStruct* cell);

  Xbyak::CodeGenerator* add_entry(std::uintptr_t address,
                                  std::unique_ptr<Xbyak::CodeGenerator> c) {
    return code_generators_.insert_or_assign(address, std::move(c))
        .first->second.get();
  }

  template <typename CodeT, typename... Args>
  Xbyak::CodeGenerator* add_entry(std::uintptr_t address, Args... args) {
    return add_entry(address, std::make_unique<CodeT>(address, args...));
  }

  template <typename CodeT, typename... Args>
  Xbyak::CodeGenerator* add_virtual(int index, std::uintptr_t address,
                                    Args... args) {
    return add_entry(address, std::make_unique<CodeT>(index, args...));
  }

  // FIXME: is this unused?
  template <typename E>
  void add_entry() {
    code_generators_[E::ptr] =
        std::make_unique<typename E::gen_t>(E::ptr, E::stack_size);
  }

  Xbyak::CodeGenerator* find_codegen(u32 address);

  codegen_store& code_generators();

  util::acquire_t<codegen_store, std::recursive_mutex>
  acquire_code_generators();

  template <typename T, typename... Args>
  auto call(Args... args) {
    return T::call(this, args...);
  }

 private:
  std::map<u32, std::unique_ptr<Xbyak::CodeGenerator>> code_generators_;
  std::recursive_mutex mut_code_generators_;
};

struct VirtualCall : Xbyak::CodeGenerator {
  explicit VirtualCall(u32 vtable_index, std::size_t stack_size = 0u) {
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
  explicit ThisCall(u32 p_fn, std::size_t stack_size = 0u) {
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
  explicit FastCall(u32 p_fn) {  // fn(args_begin, count)
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
  static constexpr bool IsThisCall = std::is_same_v<GenT, ThisCall>;
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
  static auto* get_function(ra2::abi::ABIGameMD* A) {
    auto* CC = A->find_codegen(addr());
    if (CC == nullptr) {
      if constexpr (IsThisCall) {
        CC = A->add_entry<GenT>(addr(), stack_size);
      } else {
        CC = A->add_entry<GenT>(addr());
      }
    }

    return CC->template getCode<CallT>();
  }

  // TODO(shmocz): be more explicit of the index param!
  template <typename... Args>
  static auto call_virtual(ra2::abi::ABIGameMD* A, FirstArg object,
                           Args... args) {
    auto p_virtual_function = vaddr(object);
    auto* C = A->find_codegen(p_virtual_function);
    if (C == nullptr) {
      C = A->add_virtual<VirtualCall>(addr(), p_virtual_function);
    }
    return C->template getCode<CallT>()(object, args...);
  }

  template <typename... Args>
  static auto call_noacquire(ra2::abi::ABIGameMD* A, Args... args) {
    if constexpr (IsVirtual) {
      return call_virtual(A, args...);
    } else {
      return get_function(A)(args...);
    }
  }

  template <typename... Args>
  static auto call(ra2::abi::ABIGameMD* A, Args... args) {
    return call_noacquire(A, args...);
  }
};

using ClickMission =
    Caller<0x6FFBE0U, bool __cdecl (*)(std::uintptr_t object, Mission m,
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
    Caller<0x5D3BA0U, bool __cdecl (*)(std::uintptr_t, const wchar_t*, int,
                                       const wchar_t*, i32, i32, u32, bool)>;

using sprintf = Caller<0x7E14B0U, void __cdecl (*)(const char*, std::uintptr_t),
                       FastCall, true>;

using CellClass_GetContainedTiberiumValue =
    Caller<0x485020U, int __cdecl (*)(std::uintptr_t)>;

using AbstractClass_WhatAmI =
    Caller<11, AbstractType __cdecl (*)(AbstractClass*), VirtualCall>;

using FactoryClass_DemandProduction =
    Caller<0x4C9C70,
           bool __cdecl (*)(FactoryClass*, TechnoTypeClass const* pType,
                            HouseClass* pOwner, bool shouldQueue)>;

using ObjectTypeClass_GetOwners =
    Caller<28, DWORD __cdecl (*)(ObjectTypeClass*), VirtualCall>;

using CellClass_IsShrouded = Caller<0x487950, bool __cdecl (*)(CellClass*)>;

using AbstractClass_GetDestination =
    Caller<19,
           CoordStruct* __cdecl (*)(AbstractClass*, CoordStruct* pCrd,
                                    TechnoClass* pDocker),
           VirtualCall>;

using SHPStruct_GetPixels =
    Caller<0x69E740U,
           unsigned char* __cdecl (*)(SHPStruct* p_this, int idxFrame)>;

int get_tiberium_value(const CellClass& cell);
int get_tiberium_type(int overlayTypeIndex);

template <typename T>
auto DVCIterator(DynamicVectorClass<T>* I) {
  return utility::ArrayIterator(I->Items, I->Count);
}

}  // namespace abi
}  // namespace ra2
