import asyncio
import logging as lg
import random
import unittest

import numpy as np
from ra2yrproto import ra2yr

from pyra2yr.manager import PlaceStrategy
from pyra2yr.state_manager import ObjectEntry
from pyra2yr.test_util import MyManager
from pyra2yr.util import array2coord, pdist, setup_logging

setup_logging(level=lg.DEBUG)


PT_CONNIE = r"Conscript"
PT_CONYARD = r"Yard"
PT_MCV = r"Construction\s+Vehicle"
PT_SENTRY = r"Sentry\s+Gun"
PT_SOV_BARRACKS = r"Soviet\s+Barracks"
PT_TESLA_REACTOR = r"Tesla\s+Reactor"
PT_TESLA_TROOPER = r"Shock\s+Trooper"
PT_ENGI = r"Soviet\s+Engineer"
PT_OIL = r"Oil\s+Derrick"
PT_SOV_WALL = r"Soviet\s+Wall"


# TODO(shmocz): get rid of task groups
class MultiTest(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.poll_frequency = 30
        self.fetch_state_timeout = 10.0
        self.managers: list[MyManager] = []
        for i in range(2):
            M = MyManager(
                port=14521 + i,
                poll_frequency=self.poll_frequency,
                fetch_state_timeout=self.fetch_state_timeout,
            )
            M.start()
            self.managers.append(M)

    async def deploy_mcvs(self):
        for M in self.managers:
            o = next(
                M.state.query_objects(p=PT_MCV, h=M.state.current_player())
            )
            await M.M.deploy(o.o)
        # wait until deployed
        await self.managers[0].wait_state(
            lambda: len(
                list(self.managers[0].state.query_objects(p=PT_CONYARD))
            )
            == len(self.managers)
        )
        await self.managers[0].wait_state(
            lambda: all(
                o.get().current_mission == ra2yr.Mission_Guard
                for o in self.managers[0].state.query_objects(p=PT_CONYARD)
            )
        )

    async def check_defense_structure_attack(self, M: MyManager):
        o_mcv = next(
            M.state.query_objects(p=PT_CONYARD, h=M.state.current_player())
        )
        # Build conscript and sentry
        async with asyncio.TaskGroup() as tg:
            t = tg.create_task(M.produce_unit(PT_CONNIE))
            o_sentry = await M.produce_and_place(
                M.get_unique_tc(PT_SENTRY), o_mcv.coordinates
            )
            o_con = await t
            await M.M.attack(objects=o_sentry.o, target_object=o_con.o)

    async def check_engi_capture(
        self, M: MyManager, engi: ObjectEntry, target: ObjectEntry
    ):
        self.assertNotEqual(
            target.get().pointer_house, engi.get().pointer_house
        )
        # Capture
        await M.M.capture(objects=engi.get(), target=target.get())
        # Wait until captured
        await M.wait_state(
            lambda: target.get().pointer_house == engi.get().pointer_house
        )
        self.assertEqual(target.get().pointer_house, engi.get().pointer_house)

    async def check_capture(self, M: MyManager):
        # Build engineer
        o_engi = await M.produce_unit(PT_ENGI)

        # Get nearest oil
        oils = list(M.state.query_objects(p=PT_OIL))
        dists = pdist(
            o_engi.coordinates, np.array([o.coordinates for o in oils]), axis=1
        )
        ix = np.argsort(dists)[0]
        await self.check_engi_capture(M, o_engi, oils[ix])

    async def check_repair_building(
        self, M: MyManager, src: ObjectEntry, dst: ObjectEntry
    ):
        self.assertLess(dst.health, 1.0)
        await M.M.repair(obj=src.get(), target=dst.get())
        await M.wait_state(lambda: dst.health == 1.0)
        self.assertEqual(dst.health, 1.0)

    async def do_check_repair_building(self, M: MyManager, num_attackers=5):
        attackers = []
        th = 0.7
        for _ in range(num_attackers):
            obj = await M.produce_unit(PT_CONNIE)
            attackers.append(obj.get())
        engi = await M.produce_unit(PT_ENGI)

        o_tesla = next(
            M.state.query_objects(
                p=PT_TESLA_REACTOR, h=M.state.current_player()
            )
        )
        await M.M.attack(objects=attackers, target_object=o_tesla.get())
        await M.wait_state(lambda: o_tesla.health < th)

        self.assertLess(o_tesla.health, th)
        await M.M.stop(objects=attackers)
        await self.check_repair_building(M, engi, o_tesla)

    async def sell_all_buildings(self, M: MyManager):
        for o in M.state.query_objects(
            h=M.state.current_player(), a=ra2yr.ABSTRACT_TYPE_BUILDING
        ):
            await M.M.sell(objects=o.get())

    async def do_build_stuff(self):
        for bname in [PT_TESLA_REACTOR, PT_SOV_BARRACKS]:
            async with asyncio.TaskGroup() as tg:
                for m in self.managers:
                    o_mcv = next(
                        m.state.query_objects(
                            p=PT_CONYARD, h=m.state.current_player()
                        )
                    )
                    tg.create_task(
                        m.produce_and_place(
                            m.get_unique_tc(bname), o_mcv.coordinates
                        )
                    )

    async def do_build_sell_walls(self, M: MyManager):
        o_mcv = next(
            M.state.query_objects(p=PT_CONYARD, h=M.state.current_player())
        )
        # Get corner coordinates for walls
        D = await M.get_map_data()
        I = D.ind2sub(
            np.array(
                [
                    c.index
                    for c in D.m.cells
                    if o_mcv.get().pointer_self
                    in [q.pointer_self for q in c.objects]
                ]
            )
        )
        # Get corners
        B = D.bbox(I) * 256

        tc_wall = next(
            M.state.query_type_class(
                p=PT_SOV_WALL, abstract_type=ra2yr.ABSTRACT_TYPE_BUILDINGTYPE
            )
        )
        # Build walls to corners
        for c in B:
            await M.produce_and_place(
                tc_wall, c, strategy=PlaceStrategy.ABSOLUTE
            )

        # Get wall cells
        tc_wall_ol = next(
            M.state.query_type_class(
                p=PT_SOV_WALL, abstract_type=ra2yr.ABSTRACT_TYPE_OVERLAYTYPE
            )
        )
        D = await M.get_map_data()
        I = (
            D.ind2sub(
                np.array(
                    [
                        c.index
                        for c in D.m.cells
                        if c.overlay_type_index == tc_wall_ol.array_index
                    ]
                )
            )
            * 256
        )
        I = np.c_[I, np.ones((I.shape[0], 1)) * o_mcv.coordinates[2]]

        # Sell walls
        # FIXME: check if z-coordintate fucks this up
        for c in I:
            await M.M.sell_walls(coordinates=array2coord(c))
            await asyncio.sleep(0.1)
            lg.info("OK")

        # Check that no walls exist

    async def test_multi_client(self):
        random.seed(1234)

        for M in self.managers:
            await M.M.wait_game_to_begin()

        await self.deploy_mcvs()
        await self.do_build_stuff()

        M = self.managers[0]
        await self.do_build_sell_walls(M)
        await self.do_check_repair_building(M)
        await self.check_defense_structure_attack(M)
        await self.check_capture(M)
        await self.sell_all_buildings(M)
        await M.M.wait_game_to_exit()

        # TODO(shmocz): check record file


if __name__ == "__main__":
    unittest.main()
