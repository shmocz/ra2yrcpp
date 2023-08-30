import unittest
import logging
import re

from ra2yrproto import ra2yr, commands_yr

from pyra2yr.manager import Manager
from pyra2yr.test_util import check_config, BaseGameTest
from pyra2yr.util import (
    Clock,
    coord2cell,
    coord2tuple,
    pdist,
    tuple2coord,
)

logging.basicConfig(level=logging.DEBUG)
debug = logging.debug
info = logging.info


class MyManager(Manager):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.ttc_map = {}

    async def check_move_garbage(self, addrs=None):
        if not addrs:
            addrs = [o.pointer_self for o in self.state.objects]
        tgt_coords = tuple2coord(tuple(-256 * 1000 for i in range(3)))
        for i in range(10):
            await self.M.move(
                object_addresses=addrs[:1],
                coordinates=tgt_coords,
            )

    async def check_garbage_place(self, heap_id=None):
        tgt_coords = tuple2coord(tuple(256 * 1000 for i in range(3)))
        for i in range(1):
            # place
            await self.M.place_building(
                heap_id=heap_id,
                is_naval=False,
                location=tgt_coords,
            )

    async def check_garbage_place_query(
        self, coords=None, tc_pointer=None, p_player=None
    ):
        for p in [1, p_player]:
            await self.M.get_place_locations(coords, tc_pointer, p, 128, 128)

    async def check_produce_garbage(self):
        for i in range(0, 30):
            await self.M.produce(rtti_id=i, heap_id=1000, is_naval=False)

    async def check_select_garbage(self):
        addrs = [o.pointer_self for o in self.state.objects] + [1]
        await self.M.select(object_addresses=addrs)

    async def check_deploy_garbage(self):
        addrs = [o.pointer_self for o in self.state.objects]
        for a in addrs:
            await self.M.deploy(a)

    async def check_local_deploy(self):
        addrs = [o.pointer_self for o in self.state.objects] + [1]
        await self.M.unit_command(
            object_addresses=addrs, action=commands_yr.ACTION_DEPLOY
        )

    async def check_sell(self):
        p_player = [p.self for p in self.state.houses if p.current_player][0]
        addrs = [
            o.pointer_self
            for o in self.state.objects
            if o.pointer_house != p_player
        ]
        await self.M.sell_buildings(object_addresses=addrs)

    async def check_garbage_click_event(self):
        p_player = [p.self for p in self.state.houses if p.current_player][0]
        addrs = [
            o.pointer_self
            for o in self.state.objects
            if o.pointer_house != p_player
        ]
        for e in ra2yr.NetworkEvent.values():
            await self.M.click_event(
                object_addresses=addrs, event=ra2yr.NETWORK_EVENT_Archive
            )


class InvalidDataTest(BaseGameTest):
    def get_manager_class(self):
        return MyManager

    async def test_invalid_data(self):
        M = self.M
        U = M.M
        X = Clock()

        info("wait game to begin")
        await U.wait_game_to_begin()
        info("state=ingame, players=%s", M.state.houses)
        await check_config(U)
        await self.check_record_output_defined()

        # MCV TC's
        tc_mcv = [
            t.pointer_self
            for t in M.type_classes
            if re.search(r"Vehicle", t.name)
        ]

        tc_conyard = [
            t.pointer_self for t in M.type_classes if re.search(r"Yard", t.name)
        ]

        tc_tesla = [
            t for t in M.type_classes if re.search(r"Tesla\s+Reactor", t.name)
        ]
        self.assertEqual(len(tc_tesla), 1)
        tc_tesla = tc_tesla[0]
        debug("houses %s", self.M.state.houses)

        p_player = [p.self for p in M.state.houses if p.current_player][0]
        o_mcv = [
            o
            for o in M.state.objects
            if o.pointer_technotypeclass in tc_mcv
            and o.pointer_house == p_player
        ]
        debug("mcv %s", o_mcv)
        assert len(o_mcv) == 1
        o_mcv = o_mcv[0]

        X.tic()
        await self.M.check_select_garbage()
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
        await M.check_move_garbage([o_mcv.pointer_self] * 2)
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
        await self.M.check_local_deploy()
        await U.deploy(o_mcv.pointer_self)
        await self.M.check_deploy_garbage()

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
        await self.M.check_deploy_garbage()

        # Get map data
        res = await U.read_value(map_data=ra2yr.MapData())

        # produce event
        info("produce event")
        await M.check_produce_garbage()
        await U.produce_building(heap_id=tc_tesla.array_index, is_naval=False)

        # wait until done
        await M.wait_state(
            lambda: M.state.factories
            and all([o.progress_timer == 54 for o in M.state.factories])
        )

        await M.check_garbage_place_query(
            cur_coords, tc_tesla.pointer_self, p_player
        )

        res = await U.get_place_locations(
            cur_coords, tc_tesla.pointer_self, p_player, 15, 15
        )

        coords = res.coordinates
        # get cell furthest away and place
        dists = [
            (i, pdist(cur_coords, coord2tuple(c))) for i, c in enumerate(coords)
        ]
        debug("dists=%s", dists)
        i0, _ = sorted(dists, key=lambda x: x[1], reverse=True)[0]

        # place
        await M.check_garbage_place(tc_tesla.array_index)
        await U.place_building(
            heap_id=tc_tesla.array_index,
            is_naval=False,
            location=coords[i0],
        )
        await M.check_garbage_place(tc_tesla.array_index)

        # send message
        for i in range(5):
            await U.add_message(
                message="TESTING",
                duration_frames=0x96,
                color=ra2yr.ColorScheme_Red,
            )

        def my_buildings():
            return [
                o
                for o in M.state.objects
                if o.pointer_house == p_player
                and o.object_type == ra2yr.ABSTRACT_TYPE_BUILDING
                and not o.in_limbo
            ]

        # wait until we have tesla ready
        await M.wait_state(lambda: len(my_buildings()) > 1)

        await M.check_sell()
        await M.check_garbage_click_event()

        # sell all buildings
        await U.sell_buildings(
            [
                o.pointer_self
                for o in M.state.objects
                if o.pointer_house == p_player
            ]
        )

        await U.produce_building(heap_id=tc_tesla.array_index, is_naval=False)

        debug("wait game to exit")
        await U.wait_game_to_exit()


if __name__ == "__main__":
    unittest.main()
