#include "commands_game.hpp"

#include "ra2yrproto/commands_game.pb.h"
#include "ra2yrproto/ra2yr.pb.h"

#include "command/is_command.hpp"
#include "hooks_yr.hpp"
#include "logging.hpp"
#include "ra2/abi.hpp"
#include "ra2/common.hpp"
#include "ra2/state_context.hpp"
#include "ra2/yrpp_export.hpp"
#include "types.h"

#include <fmt/core.h>

#include <cstdint>

#include <stdexcept>
#include <utility>

using namespace ra2yrcpp::commands_game;

using ra2yrcpp::command::get_async_cmd;
using ra2yrcpp::command::get_cmd;
using ra2yrcpp::hooks_yr::get_gameloop_command;

namespace r2p = ra2yrproto::ra2yr;

struct UnitOrderCtx {
  UnitOrderCtx(ra2::StateContext* ctx, const ra2yrproto::commands::UnitOrder* o)
      : ctx_(ctx), o_(o) {}

  const ra2yrproto::commands::UnitOrder& uo() { return *o_; }

  std::uintptr_t p_obj() const {
    return src_object_ == nullptr ? 0U : src_object_->pointer_self();
  }

  void click_event(ra2yrproto::ra2yr::NetworkEvent e) {
    ctx_->abi_->ClickEvent(p_obj(), e);
  }

  bool requires_source_object() {
    return !(uo().action() == r2p::UnitAction::UNIT_ACTION_SELL_CELL);
  }

  bool requires_target_object() {
    switch (uo().action()) {
      case r2p::UnitAction::UNIT_ACTION_STOP:
        return false;
      default:
        return true;
    }
    return true;
  }

  bool requires_target_cell() {
    switch (uo().action()) {
      case r2p::UnitAction::UNIT_ACTION_STOP:
        return false;
      case r2p::UnitAction::UNIT_ACTION_CAPTURE:
        return false;
      case r2p::UnitAction::UNIT_ACTION_REPAIR:
        return false;
      default:
        return true;
    }
    return true;
  }

  // Return true if source object's current mission is invalid for execution of
  // the UnitAction.
  bool is_illegal_mission(ra2yrproto::ra2yr::Mission m) {
    return (m == r2p::Mission::Mission_None ||
            m == r2p::Mission::Mission_Construction);
  }

  bool click_mission(ra2yrproto::ra2yr::Mission m) {
    CellClass* cell = nullptr;
    std::uintptr_t p_target = 0U;
    if (requires_target_object()) {
      p_target = uo().target_object();
    }
    // TODO(shmocz): figure out events where the cell should be null
    if (requires_target_cell()) {
      auto c = uo().coordinates();
      if (p_target != 0U && !uo().has_coordinates()) {
        c = ctx_->get_object_entry(p_target).o->coordinates();
      }
      if ((cell = ra2::get_map_cell(c)) == nullptr) {
        throw std::runtime_error("invalid cell");
      }
    }
    return ra2::abi::ClickMission::call(
        ctx_->abi_, p_obj(), static_cast<Mission>(m), p_target, cell, nullptr);
  }

  // Apply action to single object
  void unit_action() {
    using r2p::UnitAction;
    if (requires_source_object() &&
        is_illegal_mission(src_object_->current_mission())) {
      throw std::runtime_error(fmt::format("Object has illegal mission: {}",
                                           src_object_->current_mission()));
    }
    switch (uo().action()) {
      case UnitAction::UNIT_ACTION_DEPLOY:
        click_event(r2p::NETWORK_EVENT_Deploy);
        break;
      case UnitAction::UNIT_ACTION_SELL:
        click_event(r2p::NETWORK_EVENT_Sell);
        break;
      case UnitAction::UNIT_ACTION_SELL_CELL: {
        ra2yrproto::ra2yr::Event E;
        E.set_event_type(ra2yrproto::ra2yr::NETWORK_EVENT_SellCell);
        E.mutable_sell_cell()->mutable_cell()->CopyFrom(uo().coordinates());
        (void)ctx_->add_event(E);
      } break;
      case UnitAction::UNIT_ACTION_SELECT:
        (void)ctx_->abi_->SelectObject(p_obj());
        break;
      case UnitAction::UNIT_ACTION_MOVE:
        (void)click_mission(r2p::Mission_Move);
        break;
      case UnitAction::UNIT_ACTION_CAPTURE:
        (void)click_mission(r2p::Mission_Capture);
        break;
      case UnitAction::UNIT_ACTION_ATTACK:
        (void)click_mission(r2p::Mission_Attack);
        break;
      case UnitAction::UNIT_ACTION_ATTACK_MOVE:
        (void)click_mission(r2p::Mission_AttackMove);
        break;
      case UnitAction::UNIT_ACTION_REPAIR:
        (void)click_mission(r2p::Mission_Capture);
        break;
      case UnitAction::UNIT_ACTION_STOP:
        (void)click_mission(r2p::Mission_Stop);
        break;

      default:
        throw std::runtime_error("invalid unit action");
        break;
    }
  }

  void perform() {
    if (uo().action() == r2p::UnitAction::UNIT_ACTION_SELL_CELL) {
      unit_action();
    } else {
      for (const auto k : uo().object_addresses()) {
        src_object_ = ctx_->get_object_entry([&](const auto& v) {
                            return v.o->pointer_self() == k && !v.o->in_limbo();
                          })
                          .o;
        unit_action();
      }
    }
  }

  ra2::StateContext* ctx_;
  const ra2yrproto::commands::UnitOrder* o_;
  const ra2yrproto::ra2yr::Object* src_object_{nullptr};
};

// TODO(shmocz): copy args automagically in async cmds
auto unit_order() {
  return get_cmd<ra2yrproto::commands::UnitOrder>([](auto* Q) {
    auto args = Q->command_data();

    get_gameloop_command(Q, [args](auto* cb) {
      auto* S = cb->get_state_context();
      UnitOrderCtx ctx(S, &args);
      ctx.perform();
    });
  });
}

auto produce_order() {
  return get_async_cmd<ra2yrproto::commands::ProduceOrder>([](auto* Q) {
    auto args = Q->command_data();

    get_gameloop_command(Q, [args](auto* cb) {
      auto* ctx = cb->get_state_context();
      const auto* tc = ctx->get_type_class(args.object_type().pointer_self());
      auto can_build = ra2::abi::HouseClass_CanBuild::call(
          cb->abi(),
          reinterpret_cast<HouseClass*>(ctx->current_player()->self()),
          reinterpret_cast<TechnoTypeClass*>(tc->pointer_self()), false, false);
      if (can_build != CanBuildResult::Buildable) {
        throw std::runtime_error(
            fmt::format("unbuildable {}", args.ShortDebugString()));
      }
      dprintf("can build={}", static_cast<int>(can_build));

      ra2yrproto::ra2yr::Event E;
      E.set_event_type(ra2yrproto::ra2yr::NETWORK_EVENT_Produce);
      auto* P = E.mutable_production();
      P->set_heap_id(args.object_type().array_index());
      P->set_rtti_id(static_cast<i32>(args.object_type().type()));
      (void)ctx->add_event(E);
    });
  });
}

auto place_building() {
  return get_async_cmd<ra2yrproto::commands::PlaceBuilding>([](auto* Q) {
    auto args = Q->command_data();

    get_gameloop_command(Q, [args](auto* cb) {
      auto* ctx = cb->get_state_context();
      auto& O = args.building();
      ra2::ObjectEntry OE = ctx->get_object_entry(O);
      const auto* factory = ctx->find_factory([&](const auto& v) {
        return v.completed() && v.object() == OE.o->pointer_self() &&
               v.owner() == ctx->current_player()->self();
      });
      if (factory == nullptr) {
        throw std::runtime_error(
            fmt::format("completed object {} not found from any factory",
                        O.pointer_self()));
      }

      ctx->place_building(*ctx->current_player(), *OE.tc, args.coordinates());
    });
  });
}

std::map<std::string, ra2yrcpp::command::iservice_cmd::handler_t>
ra2yrcpp::commands_game::get_commands() {
  return {
      unit_order(),     //
      produce_order(),  //
      place_building()  //
  };
}
