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
#include "ra2/state_parser.hpp"
#include "ra2/yrpp_export.hpp"
#include "types.h"

#include <fmt/core.h>

#include <cstdint>

#include <algorithm>
#include <map>
#include <stdexcept>

using ra2yrcpp::command::get_async_cmd;
using ra2yrcpp::command::get_cmd;
using ra2yrcpp::command::message_result;

using ra2yrcpp::hooks_yr::ensure_storage_value;
using ra2yrcpp::hooks_yr::get_storage;
using ra2yrcpp::hooks_yr::put_gameloop_command;

// TODO(shmocz): don't allow deploying of already deployed object
static void unit_action(const u32 p_object,
                        const ra2yrproto::commands::UnitAction a,
                        ra2::abi::ABIGameMD* abi) {
  using ra2yrproto::commands::UnitAction;
  switch (a) {
    case UnitAction::ACTION_DEPLOY:
      abi->DeployObject(p_object);  // NB. doesn't work online
      break;
    case UnitAction::ACTION_SELL:
      abi->SellBuilding(p_object);
      break;
    case UnitAction::ACTION_SELECT:
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

    put_gameloop_command(Q, [args](auto* it) {
      auto C = it->cb;
      auto& O = C->game_state()->objects();
      for (auto k : args.object_addresses()) {
        if (std::find_if(O.begin(), O.end(), [k](auto& v) {
              return v.pointer_self() == k && !v.in_limbo();
            }) != O.end()) {
          dprintf("clickevent {} {}", k,
                  ra2yrproto::ra2yr::NetworkEvent_Name(args.event()));
          if (!C->abi()->ClickEvent(k, args.event())) {
            throw std::runtime_error(
                fmt::format("ClickEvent failed. object={},event={}", k,
                            static_cast<int>(args.event())));
          }
        }
      }
    });
  });
}

auto unit_command() {
  return get_cmd<ra2yrproto::commands::UnitCommand>([](auto* Q) {
    auto args = Q->command_data();

    put_gameloop_command(Q, [args](auto* it) {
      auto C = it->cb;
      auto& O = C->game_state()->objects();
      if (args.action() != ra2yrproto::commands::UnitAction::ACTION_SELECT &&
          !ra2::is_local(C->game_state()->houses())) {
        throw std::runtime_error(
            fmt::format("invalid local action: {}", args.action()));
      }

      for (auto k : args.object_addresses()) {
        if (std::find_if(O.begin(), O.end(), [k](auto& v) {
              return v.pointer_self() == k;
            }) != O.end()) {
          unit_action(k, args.action(), C->abi());
        }
      }
    });
  });
}

auto create_callbacks() {
  return get_cmd<ra2yrproto::commands::CreateCallbacks>([](auto* Q) {
    auto [lk_s, s] = Q->I()->aq_storage();
    // Create ABI
    (void)ensure_storage_value<ra2::abi::ABIGameMD>(Q->I(), "abi");

    // TODO(shmocz): modify the init to just ignore the request if already done
    if (s->find(ra2yrcpp::hooks_yr::key_callbacks_yr) == s->end()) {
      ra2yrcpp::hooks_yr::init_callbacks(Q->I());
    }
    auto cbs = ra2yrcpp::hooks_yr::get_callbacks(Q->I());
    lk_s.unlock();
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
    auto [mut, s] = Q->I()->aq_storage();
    Q->command_data().mutable_state()->CopyFrom(
        get_storage(Q->I())->game_state());
  });
}

auto inspect_configuration() {
  return get_cmd<ra2yrproto::commands::InspectConfiguration>([](auto* Q) {
    auto [mut, s] = Q->I()->aq_storage();
    auto& res = Q->command_data();
    auto* cfg = ra2yrcpp::hooks_yr::ensure_configuration(Q->I());
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
    put_gameloop_command(Q, [args](auto* it) {
      auto& O = it->cb->game_state()->objects();
      for (const auto k : args.object_addresses()) {
        if (std::find_if(O.begin(), O.end(), [k](auto& v) {
              return v.pointer_self() == k && !v.in_limbo();
            }) != O.end()) {
          auto c = args.coordinates();
          if (!ra2::abi::ClickMission::call(
                  it->cb->abi(), k, static_cast<Mission>(args.event()),
                  args.target_object(),
                  MapClass::Instance.get()->TryGetCellAt(
                      CoordStruct{.X = c.x(), .Y = c.y(), .Z = c.z()}),
                  nullptr)) {
            throw std::runtime_error("ClickMission error");
          }
        }
      }
    });
  });
}

auto add_event() {
  return get_async_cmd<ra2yrproto::commands::AddEvent>([](auto* Q) {
    auto args = Q->command_data();

    put_gameloop_command(Q, [args](auto* it) {
      const auto frame_delay = args.frame_delay();
      auto frame = Unsorted::CurrentFrame + frame_delay;
      auto house_index = args.spoof() ? args.event().house_index()
                                      : HouseClass::CurrentPlayer->ArrayIndex;
      // This is how the frame is computed for protocol zero.
      if (frame_delay == 0) {
        const auto& fsr = Game::Network::FrameSendRate;
        frame = (((fsr + Unsorted::CurrentFrame - 1 + Game::Network::MaxAhead) /
                  fsr) *
                 fsr);
      }
      // Set the frame to negative value to indicate that house index and
      // frame number should be spoofed
      frame = frame * (args.spoof() ? -1 : 1);

      EventClass E(static_cast<EventType>(args.event().event_type()), false,
                   static_cast<char>(house_index), static_cast<u32>(frame));

      const auto ts = it->cb->abi()->timeGetTime();
      if (args.event().has_production()) {
        auto& ev = args.event().production();
        if (ra2::find_type_class(
                it->cb->type_classes(),
                static_cast<ra2yrproto::ra2yr::AbstractType>(ev.rtti_id()),
                ev.heap_id()) == nullptr) {
          throw std::runtime_error(fmt::format("invalid id: RTTI={},heap={}",
                                               ev.rtti_id(), ev.heap_id()));
        }
        E.Data.Production = {.RTTI_ID = ev.rtti_id(),
                             .Heap_ID = ev.heap_id(),
                             .IsNaval = ev.is_naval()};

      } else if (args.event().has_place()) {
        auto& ev = args.event().place();
        auto loc = ev.location();
        auto S = CoordStruct{.X = loc.x(), .Y = loc.y(), .Z = loc.z()};
        E.Data.Place = {.RTTIType = static_cast<AbstractType>(ev.rtti_type()),
                        .HeapID = ev.heap_id(),
                        .IsNaval = ev.is_naval(),
                        .Location = CellClass::Coord2Cell(S)};
      }
      if (!EventClass::AddEvent(E, ts)) {
        throw std::runtime_error("failed to add event");
      }

      auto r = message_result<ra2yrproto::commands::AddEvent>(it->cmd);
      auto* ev = r.mutable_event();
      ev->CopyFrom(args.event());
      ev->set_timing(ts);
      it->cmd->command_data()->M.PackFrom(r);
    });
  });
}

auto place_query() {
  return get_async_cmd<ra2yrproto::commands::PlaceQuery>([](auto* Q) {
    auto args = Q->command_data();
    if (static_cast<unsigned int>(args.coordinates().size()) >
        cfg::PLACE_QUERY_MAX_LENGTH) {
      args.mutable_coordinates()->DeleteSubrange(
          cfg::PLACE_QUERY_MAX_LENGTH,
          args.coordinates().size() - cfg::PLACE_QUERY_MAX_LENGTH);
      wrprintf("truncated place query to size {}", args.coordinates().size());
    }

    put_gameloop_command(Q, [args](auto* it) {
      auto [C, cmd, fn] = *it;
      auto A = ra2::abi::DVCIterator(TechnoTypeClass::Array.get());
      auto B = std::find_if(A.begin(), A.end(), [args](auto* p) {
        return reinterpret_cast<u32>(p) == args.type_class();
      });
      // Get HouseClass
      auto& H = C->game_state()->houses();

      // TODO(shmocz): make helper method
      auto house = std::find_if(
          H.begin(), H.end(), [](const auto& h) { return h.current_player(); });
      if (args.house_class() != 0U) {
        house = std::find_if(H.begin(), H.end(), [args](const auto& h) {
          return h.self() == args.house_class();
        });
      }

      if (house == H.end()) {
        throw std::runtime_error(
            fmt::format("invalid house {}", args.house_class()));
      }

      auto r = message_result<ra2yrproto::commands::PlaceQuery>(cmd);
      ra2yrproto::commands::PlaceQuery arg;
      arg.CopyFrom(r);
      r.clear_coordinates();

      // Call for each cell
      if (B != A.end()) {
        for (auto& c : args.coordinates()) {
          auto coords = CoordStruct{.X = c.x(), .Y = c.y(), .Z = c.z()};
          auto cell_s = CellClass::Coord2Cell(coords);
          if (cell_s.X < 0 || cell_s.Y < 0) {
            continue;
          }
          auto* cs = reinterpret_cast<CellStruct*>(&cell_s);

          auto p_DisplayClass = 0x87F7E8u;
          if (C->abi()->DisplayClass_Passes_Proximity_Check(
                  p_DisplayClass, reinterpret_cast<BuildingTypeClass*>(*B),
                  house->array_index(), cs) &&
              C->abi()->BuildingTypeClass_CanPlaceHere(
                  reinterpret_cast<std::uintptr_t>(*B), cs, house->self())) {
            auto* cnew = r.add_coordinates();
            cnew->CopyFrom(c);
          }
        }
      } else {
        throw std::runtime_error(
            fmt::format("nonexistent TypeClass {}", args.type_class()));
      }
      // copy results
      cmd->command_data()->M.PackFrom(r);
    });
  });
}

auto send_message() {
  return get_cmd<ra2yrproto::commands::AddMessage>([](auto* Q) {
    auto args = Q->command_data();

    put_gameloop_command(Q, [args](auto* it) {
      it->cb->abi()->AddMessage(1, args.message(), args.color(), 0x4046,
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
                       get_storage(Q->I())->mutable_map_data());
    } else {
      // TODO(shmocz): use oneof
      ra2yrcpp::protocol::copy_field(D, get_storage(Q->I()), fld);
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
