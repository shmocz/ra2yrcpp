#include "commands_yr.hpp"

#include "ra2yrproto/commands_yr.pb.h"
#include "ra2yrproto/ra2yr.pb.h"

#include "command/is_command.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "hooks_yr.hpp"
#include "logging.hpp"
#include "protocol/helpers.hpp"
#include "ra2/abi.hpp"
#include "ra2/common.hpp"
#include "ra2/state_parser.hpp"
#include "ra2/yrpp_export.hpp"
#include "types.h"

#include <fmt/core.h>

#include <algorithm>
#include <map>
#include <stdexcept>

using ra2yrcpp::command::get_async_cmd;
using ra2yrcpp::command::get_cmd;
using ra2yrcpp::command::message_result;

using ra2yrcpp::hooks_yr::ensure_storage_value;
using ra2yrcpp::hooks_yr::get_data;
using ra2yrcpp::hooks_yr::get_gameloop_command;

// TODO(shmocz): don't allow deploying of already deployed object
static void unit_action(const u32 p_object,
                        const ra2yrproto::ra2yr::UnitAction a,
                        ra2::abi::ABIGameMD* abi) {
  using ra2yrproto::ra2yr::UnitAction;
  switch (a) {
    case UnitAction::UNIT_ACTION_DEPLOY:
      abi->DeployObject(p_object);  // NB. doesn't work online
      break;
    case UnitAction::UNIT_ACTION_SELL:
      abi->SellBuilding(p_object);
      break;
    case UnitAction::UNIT_ACTION_SELECT:
      (void)abi->SelectObject(p_object);
      break;
    default:
      break;
  }
}

namespace cmd {
auto click_event() {
  return get_cmd<ra2yrproto::commands::ClickEvent>([](auto* Q) {
    auto args = Q->command_data();

    get_gameloop_command(Q, [args](auto* cb) {
      for (auto k : args.object_addresses()) {
        auto* OE = cb->get_state_context()->get_object([&](const auto& v) {
          return v.pointer_self() == k && !v.in_limbo();
        });
        if (OE == nullptr) {
          continue;
        }

        dprintf("clickevent {} {}", k,
                ra2yrproto::ra2yr::NetworkEvent_Name(args.event()));
        if (!cb->abi()->ClickEvent(k, args.event())) {
          throw std::runtime_error(
              fmt::format("ClickEvent failed. object={},event={}", k,
                          static_cast<int>(args.event())));
        }
      }
    });
  });
}

auto unit_command() {
  return get_cmd<ra2yrproto::commands::UnitCommand>([](auto* Q) {
    auto args = Q->command_data();

    get_gameloop_command(Q, [args](auto* cb) {
      if (args.action() != ra2yrproto::ra2yr::UnitAction::UNIT_ACTION_SELECT &&
          !ra2::is_local(cb->game_state()->houses())) {
        throw std::runtime_error(
            fmt::format("invalid local action: {}", args.action()));
      }

      for (auto k : args.object_addresses()) {
        if (cb->get_state_context()->get_object([&](const auto& v) {
              return v.pointer_self() == k && !v.in_limbo();
            }) != nullptr) {
          unit_action(k, args.action(), cb->abi());
        }
      }
    });
  });
}

auto create_callbacks() {
  return get_cmd<ra2yrproto::commands::CreateCallbacks>([](auto* Q) {
    auto [lk_s, s] = Q->I()->aq_storage();
    // Create main game data structure
    // TODO(shmocz): initialize elsewhere
    ra2yrcpp::hooks_yr::init_callbacks(get_data(Q->I()));
    auto cbs = ra2yrcpp::hooks_yr::get_callbacks(Q->I());
    auto [lk, hhooks] = Q->I()->aq_hooks();
    for (auto& [k, v] : *cbs) {
      auto target = v->target();
      auto h = std::find_if(hhooks->begin(), hhooks->end(), [&](auto& a) {
        return (a.second.name() == target);
      });
      if (h == hhooks->end()) {
        throw ra2yrcpp::general_error(fmt::format("No such hook {}", target));
      }

      const std::string hook_name = k;
      auto& tmp_cbs = h->second.callbacks();
      // TODO(shmocz): throw standard exception
      if (std::find_if(tmp_cbs.begin(), tmp_cbs.end(), [&hook_name](auto& a) {
            return a.name == hook_name;
          }) != tmp_cbs.end()) {
        throw ra2yrcpp::general_error(fmt::format(
            "Hook {} already has a callback {}", target, hook_name));
      }

      iprintf("add callback, target={} cb={}", target, hook_name);
      auto cb = v.get();
      cb->add_to_hook(&h->second, Q->I());
    }
  });
}

auto get_game_state() {
  return get_cmd<ra2yrproto::commands::GetGameState>([](auto* Q) {
    Q->I()->lock_storage();
    auto* D = get_data(Q->I());
    // Unpause game if single-step mode.
    if (D->cfg.single_step() && D->game_paused.get()) {
      Q->I()->unlock_storage();
      D->game_paused.wait(true);
      Q->I()->lock_storage();
    }

    Q->command_data().mutable_state()->CopyFrom(D->sv.game_state());
    if (D->cfg.single_step()) {
      D->game_paused.store(false);
    }
    Q->I()->unlock_storage();
  });
}

auto inspect_configuration() {
  return get_cmd<ra2yrproto::commands::InspectConfiguration>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto& res = Q->command_data();
    auto* cfg = &ra2yrcpp::hooks_yr::get_data(Q->I())->cfg;
    cfg->MergeFrom(Q->command_data().config());
    res.mutable_config()->CopyFrom(*cfg);
  });
}

// NB. CellClicked not called for moving units, but for attack (and what
// else?) ClickedMission seems to be used for various other events
auto mission_clicked() {
  return get_cmd<ra2yrproto::commands::MissionClicked>([](auto* Q) {
    auto args = Q->command_data();

    // TODO(shmocz): Figure out a way to avoid copies. the command protobuf
    // message is already stored in the Command object, but since it's of type
    // `Any`, a message object with appropriate type needs to be allocated for
    // unpacking.
    get_gameloop_command(Q, [args](auto* cb) {
      for (const auto k : args.object_addresses()) {
        auto* OE = cb->get_state_context()->get_object([&](const auto& v) {
          return v.pointer_self() == k && !v.in_limbo();
        });
        if (OE == nullptr) {
          continue;
        }

        auto c = args.coordinates();
        if (!ra2::abi::ClickMission::call(
                cb->abi(), k, static_cast<Mission>(args.event()),
                args.target_object(), ra2::get_map_cell(c), nullptr)) {
          throw std::runtime_error("ClickMission error");
        }
      }
    });
  });
}

auto add_event() {
  return get_async_cmd<ra2yrproto::commands::AddEvent>([](auto* Q) {
    auto args = Q->command_data();
    auto* cmd = Q->c;

    get_gameloop_command(Q, [args, cmd](auto* it) {
      auto E = it->get_state_context()->add_event(
          args.event(), args.frame_delay(), args.spoof(), false);

      auto r = message_result<ra2yrproto::commands::AddEvent>(cmd);
      auto* ev = r.mutable_event();
      ev->CopyFrom(args.event());
      // FIXME(shmocz): properly get the timing
      ev->set_timing(E.timing);
      cmd->command_data()->M.PackFrom(r);
    });
  });
}

auto place_query() {
  return get_async_cmd<ra2yrproto::commands::PlaceQuery>([](auto* Q) {
    auto args = Q->command_data();
    auto* C = Q->c;
    if (ra2yrcpp::protocol::truncate(args.mutable_coordinates(),
                                     cfg::PLACE_QUERY_MAX_LENGTH)) {
      wrprintf("truncated place query to size {}", args.coordinates().size());
    }

    get_gameloop_command(Q, [args, C](auto* cb) {
      auto* SC = cb->get_state_context();
      auto* B = SC->get_type_class(args.type_class());
      auto* house = SC->get_house(args.house_class());
      if (house == nullptr) {
        throw std::runtime_error(
            fmt::format("invalid house {}", args.house_class()));
      }

      auto r = message_result<ra2yrproto::commands::PlaceQuery>(C);
      ra2yrproto::commands::PlaceQuery arg;
      arg.CopyFrom(r);
      r.clear_coordinates();

      // Call for each cell
      if (B != nullptr) {
        for (auto& c : args.coordinates()) {
          auto cell_s = ra2::coord_to_cell(c);
          if (cell_s.X < 0 || cell_s.Y < 0) {
            continue;
          }
          auto* cs = reinterpret_cast<CellStruct*>(&cell_s);

          auto p_DisplayClass = 0x87F7E8u;
          if (cb->abi()->DisplayClass_Passes_Proximity_Check(
                  p_DisplayClass,
                  reinterpret_cast<BuildingTypeClass*>(B->pointer_self()),
                  house->array_index(), cs) &&
              cb->abi()->BuildingTypeClass_CanPlaceHere(B->pointer_self(), cs,
                                                        house->self())) {
            auto* cnew = r.add_coordinates();
            cnew->CopyFrom(c);
          }
        }
      }
      // copy results
      C->command_data()->M.PackFrom(r);
    });
  });
}

auto send_message() {
  return get_cmd<ra2yrproto::commands::AddMessage>([](auto* Q) {
    auto args = Q->command_data();

    get_gameloop_command(Q, [args](auto* cb) {
      cb->abi()->AddMessage(1, args.message(), args.color(), 0x4046,
                            args.duration_frames(), false);
    });
  });
}

static void convert_map_data(ra2yrproto::ra2yr::MapDataSoA* dst,
                             ra2yrproto::ra2yr::MapData* src) {
  const auto sz = src->cells().size();

  for (int i = 0U; i < sz; i++) {
    dst->add_land_type(src->cells(i).land_type());
    dst->add_radiation_level(src->cells(i).radiation_level());
    dst->add_height(src->cells(i).height());
    dst->add_level(src->cells(i).level());
    dst->add_overlay_data(src->cells(i).overlay_data());
    dst->add_tiberium_value(src->cells(i).tiberium_value());
    dst->add_shrouded(src->cells(i).shrouded());
    dst->add_passability(src->cells(i).passability());
  }
  dst->set_map_width(src->width());
  dst->set_map_height(src->height());
}

// windows.h idiotism
#undef GetMessage

///
/// Read a protobuf message from storage determined by command argument type.
auto read_value() {
  return get_cmd<ra2yrproto::commands::ReadValue>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto& A = Q->command_data();
    // find the first field that's been set
    auto sf = ra2yrcpp::protocol::find_set_fields(A.data());
    if (sf.empty()) {
      throw std::runtime_error("no field specified");
    }
    auto* fld = sf[0];
    auto* D = A.mutable_data();

    if (fld->name() == "map_data_soa") {
      convert_map_data(D->mutable_map_data_soa(),
                       get_data(Q->I())->sv.mutable_map_data());
    } else {
      // TODO(shmocz): use oneof
      ra2yrcpp::protocol::copy_field(D, &get_data(Q->I())->sv, fld);
    }
  });
}

}  // namespace cmd

std::map<std::string, ra2yrcpp::command::iservice_cmd::handler_t>
commands_yr::get_commands() {
  return {
      cmd::click_event(),            //
      cmd::unit_command(),           //
      cmd::create_callbacks(),       //
      cmd::get_game_state(),         //
      cmd::inspect_configuration(),  //
      cmd::mission_clicked(),        //
      cmd::add_event(),              //
      cmd::place_query(),            //
      cmd::send_message(),           //
      cmd::read_value(),             //
  };
}
