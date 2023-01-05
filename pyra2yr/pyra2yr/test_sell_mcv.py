import asyncio
import logging
import re
import gzip
import os
from typing import List
import datetime


from ra2yrproto import commands_yr, commands_builtin
from ra2yrproto import ra2yr

from pyra2yr.manager import Manager, read_protobuf_messages
from pyra2yr.network import create_app, log_exceptions

logging.basicConfig(level=logging.DEBUG)
debug = logging.debug
info = logging.info


class Clock:
    def __init__(self):
        self.t = None

    def tick(self):
        self.t = datetime.datetime.now()

    def tock(self):
        return (datetime.datetime.now() - self.t).total_seconds()


class StateUtil:
    def __init__(self, type_classes: List[ra2yr.TypeClass]):
        self.type_classes = type_classes
        self.state = None

    def set_state(self, state: ra2yr.GameState):
        self.state = state

    def get_house(self, name: str) -> ra2yr.House:
        return next(h for h in self.state.houses if h.name == name)

    def get_units(self, house_name: str) -> ra2yr.Object:
        h = self.get_house(house_name)
        return (u for u in self.state.objects if u.pointer_house == h.self)

    def get_production(self, house_name: str) -> ra2yr.Factory:
        h = self.get_house(house_name)
        return (f for f in self.state.factories if f.owner == h.self)


def unixpath(p: str):
    return re.sub(r"^\w:", "", re.sub(r"\\+", "/", p))


def coord2cell(x):
    if isinstance(x, tuple):
        return tuple(coord2cell(v) for v in x)
    return int(x / 256)


def coord2tuple(x):
    return tuple(getattr(x, k) for k in "xyz")


def tuple2coord(x):
    return ra2yr.Coordinates(**{k: x[i] for i, k in enumerate("xyz")})


async def mcv_sell(app=None):
    M = Manager(poll_frequency=30)
    M.start()
    X = Clock()

    await M.wait_state(lambda: M.state.stage == ra2yr.STAGE_INGAME)

    # Get TC's
    res_tc = await M.run(commands_yr.GetTypeClasses())
    M.type_classes = res_tc.result.classes

    # MCV TC's
    tc_mcv = [
        t.pointer_self for t in M.type_classes if re.search(r"Vehicle", t.name)
    ]
    tc_conyard = [
        t.pointer_self for t in M.type_classes if re.search(r"Yard", t.name)
    ]

    # Get MCV pointer
    p_player = [p.self for p in M.state.houses if p.name == "player_0"][0]
    o_mcv = [
        o
        for o in M.state.objects
        if o.pointer_technotypeclass in tc_mcv and o.pointer_house == p_player
    ]
    assert len(o_mcv) == 1
    o_mcv = o_mcv[0]

    X.tick()
    await M.run(
        commands_yr.UnitCommand(),
        object_addresses=[o_mcv.pointer_self],
        action=commands_yr.ACTION_SELECT,
    )

    # Wait until selected
    await M.wait_state(
        lambda: [
            o
            for o in M.state.objects
            if o.pointer_self == o_mcv.pointer_self
            and o.pointer_house == p_player
            and o.selected
        ]
    )

    info("latency(select): %f", X.tock())

    # Get one cell down
    cur_coords = coord2tuple(o_mcv.coordinates)
    tgt_coords = tuple2coord(
        tuple(x + y * 256 for x, y in zip(cur_coords, (1, 0, 0)))
    )
    info("tgt %s", tgt_coords)

    # Move to specific cell
    await M.run(
        commands_yr.MissionClicked(),
        object_addresses=[o_mcv.pointer_self],
        event=ra2yr.Mission_Move,
        coordinates=tgt_coords,
    )

    await M.wait_state(
        lambda: [
            o
            for o in M.state.objects
            if o.pointer_self == o_mcv.pointer_self
            and coord2cell(coord2tuple(o.coordinates))
            == coord2cell(coord2tuple(tgt_coords))
        ]
    )
    X.tick()
    # deploy
    await M.run(
        commands_yr.ClickEvent(),
        object_addresses=[o_mcv.pointer_self],
        event=ra2yr.NETWORK_EVENT_Deploy,
    )

    # wait until there's a MCV
    await M.wait_state(
        lambda: [
            o
            for o in M.state.objects
            if o.pointer_technotypeclass in tc_conyard
            and o.pointer_house == p_player
        ]
    )
    info("latency(deploy): %f", X.tock())

    o_conyard = [
        o
        for o in M.state.objects
        if o.pointer_technotypeclass in tc_conyard
        and o.pointer_house == p_player
    ][0]

    # produce event
    ev = ra2yr.Event(
        event_type=0xE,
        house_index=0,
        production=ra2yr.Event.Production(rtti_id=0x7, heap_id=0x9),
    )
    await M.run(commands_yr.AddEvent(), event=ev)

    # wait until done
    await M.wait_state(
        lambda: M.state.factories
        and all([o.progress_timer == 54 for o in M.state.factories])
    )

    # place to closest cell
    place_query_grid = []
    for i in range(1, 5):
        for j in range(1, 5):
            place_query_grid.append(
                tuple2coord(
                    tuple(x + y * 256 for x, y in zip(cur_coords, (i, j, 0)))
                )
            )

    # cell command
    res = await M.run(
        commands_yr.PlaceQuery(),
        type_class=o_mcv.pointer_technotypeclass,
        house_class=p_player,
        coordinates=place_query_grid,
    )
    assert len(res.result.coordinates) < len(place_query_grid)

    # place
    ev = ra2yr.Event(
        event_type=ra2yr.NETWORK_EVENT_Place,
        house_index=0,
        place=ra2yr.Event.Place(
            rtti_type=0x7,
            heap_id=0x9,
            is_naval=False,
            location=res.result.coordinates[0],
        ),
    )
    await M.run(commands_yr.AddEvent(), event=ev)

    # send message
    await M.run(
        commands_yr.AddMessage(),
        message="TESTING",
        duration_frames=0x96,
        color=ra2yr.ColorScheme_Red,
    )

    # input("")
    # sell all buildings
    for p in (o for o in M.state.objects if o.pointer_house == p_player):
        await M.run(
            commands_yr.ClickEvent(),
            object_addresses=[p.pointer_self],
            event=ra2yr.NETWORK_EVENT_Sell,
        )

    # cancel

    # await M.run(
    #     commands_yr.AddEvent(),
    #     event=ra2yr.Event(
    #         event_type=ra2yr.NETWORK_EVENT_Abandon,
    #         production=ra2yr.Event.Production(rtti_id=0x7, heap_id=0x9),
    #     ),
    # )
    debug("wait game to exit")
    await M.wait_state(lambda: M.state.stage != ra2yr.STAGE_EXIT_GAME)

    # verify recording
    cfg = await M.run(commands_yr.InspectConfiguration())

    res_s = await M.run(commands_builtin.GetSystemState())

    record_path = os.path.join(
        unixpath(res_s.result.state.directory),
        cfg.result.config.record_filename,
    )

    app["do_stop"].set()
    await M.stop()
    with gzip.open(record_path, "rb") as f:
        m = read_protobuf_messages(f)

        # get type classes
        m0 = next(m)
        S = StateUtil(m0.object_types)
        S.set_state(m0)
        for m0 in m:
            S.set_state(m0)
            for u in S.get_units("player_0"):
                if u.deployed or u.deploying:
                    print("DEPLOY!")


async def start(app):
    t = asyncio.create_task(log_exceptions(mcv_sell(app)))
    app["task"] = t


async def stop(app):
    await app["task"]


async def test_sell_mcv(host: str):
    await create_app(destination=host, on_startup=[start], on_shutdown=[stop])


if __name__ == "__main__":
    asyncio.run(test_sell_mcv("0.0.0.0"))
