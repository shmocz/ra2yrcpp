import asyncio
import logging as lg
import re

from ra2yrproto import ra2yr

from pyra2yr.manager import Manager, ManagerUtil, CommandBuilder
from pyra2yr.util import (
    coord2tuple,
    pdist,
    setup_logging,
)


async def main():
    M = Manager(poll_frequency=30)
    M.start()
    U = ManagerUtil(M)

    await U.wait_game_to_begin()

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

    player_0 = [p for p in M.players() if p.current_player][0]
    player_1 = [p for p in M.players() if not p.current_player][0]

    # Get MCV pointer
    o_mcv = [
        o
        for o in M.state.objects
        if o.pointer_technotypeclass in tc_mcv
        and o.pointer_house == player_0.self
    ]

    assert len(o_mcv) == 1
    o_mcv = o_mcv[0]

    # Get enemy MCV
    p1_mcv = [
        o
        for o in M.state.objects
        if o.pointer_technotypeclass in tc_mcv
        and o.pointer_house == player_1.self
    ][0]
    mcv_coords = coord2tuple(p1_mcv.coordinates)

    await U.select(object_addresses=[o_mcv.pointer_self])

    # wait until selected
    await M.wait_state(
        lambda: [
            o
            for o in M.state.objects
            if o.pointer_self == o_mcv.pointer_self
            and o.pointer_house == player_0.self
            and o.selected
        ]
    )

    await U.deploy(o_mcv.pointer_self)

    # wait until there's a MCV
    await M.wait_state(
        lambda: [
            o
            for o in M.state.objects
            if o.pointer_technotypeclass in tc_conyard
            and o.pointer_house == player_0.self
        ]
    )

    # wait until opponent deploys mcv
    o_mcv = [
        o
        for o in M.state.objects
        if o.pointer_technotypeclass in tc_mcv
        and o.pointer_house == player_1.self
    ]

    await M.wait_state(
        lambda: [
            o
            for o in M.state.objects
            if o.pointer_technotypeclass in tc_conyard
            and o.pointer_house == player_1.self
        ]
    )

    C = CommandBuilder

    # produce tesla reactor as opponent
    ev_produce = C.make_produce(
        rtti_id=ra2yr.ABSTRACT_TYPE_BUILDINGTYPE,
        heap_id=tc_tesla.array_index,
        is_naval=False,
    )
    ev_produce.args.spoof = True
    ev_produce.args.frame_delay = 0
    ev_produce.args.event.house_index = player_1.array_index
    await M.run(ev_produce)

    # wait until done
    await M.wait_state(
        lambda: M.state.factories
        and all([o.progress_timer == 54 for o in M.state.factories])
    )

    res = await U.get_place_locations(
        mcv_coords, tc_tesla.pointer_self, player_1.self, 15, 15
    )

    # get cell furthest away and place
    dists = [
        (i, pdist(mcv_coords, coord2tuple(c)))
        for i, c in enumerate(res.result.coordinates)
    ]

    i0, _ = sorted(dists, key=lambda x: x[1], reverse=True)[0]

    await asyncio.sleep(2)

    # place (as other user)
    ev_place = C.make_place(
        heap_id=tc_tesla.array_index,
        is_naval=False,
        location=res.result.coordinates[i0],
    )
    ev_place.args.spoof = True
    ev_place.args.frame_delay = 0
    ev_place.args.event.house_index = player_1.array_index

    await M.run(ev_place)

    # wait for building to appear
    await M.wait_state(
        lambda: [
            o
            for o in M.state.objects
            if o.pointer_technotypeclass == tc_tesla.pointer_self
            and o.pointer_house == player_1.self
        ],
        timeout=5,
    )

    await asyncio.sleep(2)

    # sell all buildings
    for p in (o for o in M.state.objects if o.pointer_house == player_0.self):
        await U.sell_building(p.pointer_self)

    lg.info("wait game to exit")
    await U.wait_game_to_exit()
    await M.stop()


if __name__ == "__main__":
    setup_logging(level=lg.DEBUG)
    asyncio.run(main())
