import asyncio
import logging
import os
import re

from ra2yrproto import ra2yr

from pyra2yr.manager import Manager, ManagerUtil
from pyra2yr.network import logged_task
from pyra2yr.test_util import verify_recording
from pyra2yr.util import (
    Clock,
    coord2cell,
    coord2tuple,
    pdist,
    tuple2coord,
    unixpath,
)

logging.basicConfig(level=logging.DEBUG)
debug = logging.debug
info = logging.info


async def mcv_sell(app=None):
    M = Manager(poll_frequency=30)
    info("start manager")
    M.start()
    info("manager started")
    U = ManagerUtil(M)
    X = Clock()

    index_player = 0
    player_name = "player_0"

    info("wait game to begin")
    await M.wait_state(
        lambda: M.state.stage == ra2yr.STAGE_INGAME
        and M.state.current_frame > 1
        and len(M.type_classes) > 0,
        timeout=60,
    )
    info("state=ingame, players=%s", M.state.houses)

    # Get TC's
    info("num tc=%s", len(M.type_classes))

    # MCV TC's
    tc_mcv = [
        t.pointer_self for t in M.type_classes if re.search(r"Vehicle", t.name)
    ]

    tc_conyard = [
        t.pointer_self for t in M.type_classes if re.search(r"Yard", t.name)
    ]

    tc_tesla = [
        t for t in M.type_classes if re.search(r"Tesla\s+Reactor", t.name)
    ]
    assert len(tc_tesla) == 1
    tc_tesla = tc_tesla[0]
    req = [t for t in M.type_classes if t.array_index in tc_tesla.prerequisites]
    debug("tesla=%s, req=%s", tc_tesla, req)

    # Get MCV pointer
    p_player = [p.self for p in M.state.houses if p.name == player_name][0]
    o_mcv = [
        o
        for o in M.state.objects
        if o.pointer_technotypeclass in tc_mcv and o.pointer_house == p_player
    ]
    debug("mcv %s", o_mcv)
    assert len(o_mcv) == 1
    o_mcv = o_mcv[0]

    X.tic()
    debug("selecting MCV p=%d", o_mcv.pointer_self)
    await U.select(object_addresses=[o_mcv.pointer_self])

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

    info("latency(select): %f", X.toc())

    # Move one cell down
    cur_coords = coord2tuple(o_mcv.coordinates)
    tgt_coords = tuple2coord(
        tuple(x + y * 256 for x, y in zip(cur_coords, (1, 0, 0)))
    )
    info("move to %s", tgt_coords)
    await U.move(
        object_addresses=[o_mcv.pointer_self],
        coordinates=tgt_coords,
    )

    # Wait until MCV on cell
    # TODO: just wait to be deployable
    await M.wait_state(
        lambda: [
            o
            for o in M.state.objects
            if o.pointer_self == o_mcv.pointer_self
            and coord2cell(coord2tuple(o.coordinates))
            == coord2cell(coord2tuple(tgt_coords))
        ]
    )

    X.tic()
    await U.deploy(o_mcv.pointer_self)

    # wait until there's a MCV
    await M.wait_state(
        lambda: [
            o
            for o in M.state.objects
            if o.pointer_technotypeclass in tc_conyard
            and o.pointer_house == p_player
        ]
    )
    info("latency(deploy): %f", X.toc())

    # Get map data
    res = await U.read_value(map_data=ra2yr.MapData())

    # produce event
    info("produce event")
    await U.produce_building(
        house_index=index_player,
        heap_id=tc_tesla.array_index,
    )

    # wait until done
    await M.wait_state(
        lambda: M.state.factories
        and all([o.progress_timer == 54 for o in M.state.factories])
    )

    res = await U.get_place_locations(
        cur_coords, tc_tesla.pointer_self, p_player, 15, 15
    )

    # get cell furthest away and place
    dists = [
        (i, pdist(cur_coords, coord2tuple(c)))
        for i, c in enumerate(res.result.coordinates)
    ]
    debug("dists=%s", dists)
    i0, _ = sorted(dists, key=lambda x: x[1], reverse=True)[0]

    # place
    await U.place_building(
        heap_id=tc_tesla.array_index,
        is_naval=False,
        location=res.result.coordinates[i0],
    )

    # send message
    await U.add_message(
        message="TESTING", duration_frames=0x96, color=ra2yr.ColorScheme_Red
    )

    # sell all buildings
    for p in (o for o in M.state.objects if o.pointer_house == p_player):
        await U.sell_building(p.pointer_self)

    debug("wait game to exit")
    await M.wait_state(lambda: M.state.stage == ra2yr.STAGE_EXIT_GAME)

    # get record file path
    cfg = await U.inspect_configuration()
    res_s = await U.get_system_state()

    await M.stop()
    verify_recording(
        os.path.join(
            unixpath(res_s.result.state.directory),
            cfg.result.config.record_filename,
        )
    )


async def test_sell_mcv(host: str):
    t = logged_task(mcv_sell())
    await t


if __name__ == "__main__":
    asyncio.run(test_sell_mcv("0.0.0.0"))
