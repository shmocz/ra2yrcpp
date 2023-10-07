import asyncio
import logging
import os
import random
import re
import unittest

import numpy as np
from ra2yrproto import core, ra2yr

from pyra2yr.manager import PlaceStrategy
from pyra2yr.state_manager import ObjectFactory
from pyra2yr.test_util import BaseGameTest, check_config, verify_recording
from pyra2yr.util import (
    array2coord,
    coord2array,
    setup_logging,
    unixpath,
)

setup_logging(level=logging.DEBUG)
debug = logging.debug
info = logging.info


class MCVSellTest(BaseGameTest):
    async def check_record_output_defined(self):
        # Get config
        cmd_1 = await self.M.M.inspect_configuration()
        cfg1 = cmd_1.config
        self.assertNotEqual(cfg1.record_filename, "")

    async def verify_recording(self):
        # get record file path
        cfg = await self.U.inspect_configuration()
        res_s = await self.U.get_system_state()

        verify_recording(
            os.path.join(
                unixpath(res_s.state.directory),
                cfg.config.record_filename,
            )
        )

    async def produce_and_place(
        self,
        t: ra2yr.ObjectTypeClass,
        coords,
        strategy: PlaceStrategy = PlaceStrategy.FARTHEST,
    ):
        M = self.M
        U = self.U
        r = await U.start_production(t)
        self.assertEqual(r.result_code, core.ResponseCode.OK)
        obj = None
        debug("coords=%s", coords)

        # wait until corresponding type is being produced
        await M.wait_state(
            lambda: any(
                o.pointer_technotypeclass == t.pointer_self
                for (o, _) in self.get_production(self.current_player())
            )
        )

        obj, _ = next(
            x
            for x in self.get_production()
            if x[0].pointer_technotypeclass == t.pointer_self
        )

        # wait until done
        await M.wait_state(
            lambda: all(
                f.completed and o.pointer_technotypeclass == t.pointer_self
                for (o, f) in self.get_production(self.current_player())
            )
        )

        if strategy == PlaceStrategy.FARTHEST:
            place_locations = await self.get_place_locations(
                coords, next(self.sm.get_objects(t)), 15, 15
            )

            # get cell closest away and place
            dists = np.sqrt(np.sum((place_locations - coords) ** 2, axis=1))
            i0 = np.argsort(dists)[-1]
            coords = place_locations[i0]
        fac = {x.object: x for x in self.get_factories(self.current_player())}
        OF = ObjectFactory(self.sm)
        obj = OF.create_object(
            next(
                o
                for o in self.house_objects(self.current_player())
                if o.pointer_technotypeclass == t.pointer_self
                and o.pointer_self in fac
            )
        )
        r = await U.place_building(
            building=obj.o, coordinates=array2coord(coords)
        )
        if r.result_code != core.ResponseCode.OK:
            raise RuntimeError(f"place failed: {r.error_message}")
        # wait until building has been placed
        await M.wait_state(
            lambda: obj.o.pointer_self
            not in [
                q.pointer_self
                for q, _ in self.get_production(self.current_player())
            ]
        )
        return obj.o

    async def get_unique_tc(self, pattern):
        tc = list(self.sm.get_type_class_by_regex(pattern))
        self.assertEqual(len(tc), 1)
        return tc[0]

    async def map_data(self) -> ra2yr.MapData:
        res = await self.M.M.read_value(map_data=ra2yr.MapData())
        return res.data.map_data

    async def test_sell_mcv(self):
        random.seed(1234)
        M = self.M
        U = M.M
        self.U = U

        await U.wait_game_to_begin()
        await check_config(U)
        await self.check_record_output_defined()

        # Get TC's
        info("num tc=%s", len(M.type_classes))

        tc_tesla = await self.get_unique_tc(r"Tesla\s+Reactor")
        tc_conscript = await self.get_unique_tc(r"Conscript")
        tc_engi = await self.get_unique_tc(r"^Engineer$")
        tc_wall = next(
            self.sm.query_type_class(
                r"Soviet\s+Wall", ra2yr.ABSTRACT_TYPE_BUILDINGTYPE
            )
        )
        tc_rax = await self.get_unique_tc(r"Soviet\s+Barracks")

        # Get MCV object
        mcvs = [
            o
            for o in self.house_objects(self.current_player())
            if re.search(
                r"Construction\s+Vehicle",
                self.sm.ttc_map[o.pointer_technotypeclass].name,
            )
        ]
        self.assertEqual(len(mcvs), 1)
        o_mcv = mcvs[0]
        # debug(
        #     "overlay types=%s",
        #     "\n".join(
        #         msg_oneline(t)
        #         for t in self.sm.types()
        #         if t.type == ra2yr.ABSTRACT_TYPE_OVERLAYTYPE
        #     ),
        # )
        # get wall overlay type
        tc_wall_ol = next(
            self.sm.query_type_class(
                r"Soviet\s+Wall", ra2yr.ABSTRACT_TYPE_OVERLAYTYPE
            )
        )
        debug("wall overlay %s", tc_wall_ol)

        await U.select(object_addresses=[o_mcv.pointer_self])

        # Wait until selected
        await M.wait_state(lambda: [self.sm.get_object(o_mcv).selected])

        cur_coords = coord2array(o_mcv.coordinates)
        tgt_coords = cur_coords + 256 * np.array([1, 0, 0])
        debug("moving mcv")
        await U.move(
            object_addresses=[o_mcv.pointer_self],
            coordinates=array2coord(tgt_coords),
        )

        # Wait until MCV on cell
        # TODO(shmocz): just wait to be deployable
        await M.wait_state(
            lambda: np.all(
                coord2array(self.sm.get_object(o_mcv).coordinates) / 256
                == tgt_coords / 256
            ),
            timeout=5.0,
            err="MCV at target cell",
        )

        await U.deploy(o_mcv.pointer_self)

        await M.wait_state(
            lambda: [
                o
                for o in self.house_objects(self.current_player())
                if re.search(
                    r"Yard", self.sm.ttc_map[o.pointer_technotypeclass].name
                )
            ],
            err="conyard ready",
        )

        # Check that we cant build illegal buildings
        # r = await M.run_command(
        #     commands_game.ProduceOrder(
        #         object_type=tc_power, action=ra2yr.PRODUCE_ACTION_BEGIN
        #     )
        # )
        # self.assertRegex(r.error_message, r"unbuildable")

        b_tesla = await self.produce_and_place(tc_tesla, cur_coords)

        await asyncio.sleep(0.5)

        b_rax = await self.produce_and_place(tc_rax, cur_coords)

        await asyncio.sleep(0.5)

        # b_wall = await self.produce_and_place(tc_wall, cur_coords)

        # await asyncio.sleep(0.5)

        # build wall around conyard

        # sell conyard
        o_conyard = next(
            o
            for o in self.my_buildings()
            if re.search(
                r"Construction\s+Yard",
                self.sm.ttc_map[o.pointer_technotypeclass].name,
            )
        )

        # get cells that contain MCV
        W = await self.map_data()
        D = [
            i
            for i, c in enumerate(W.cells)
            if o_conyard.pointer_self in [q.pointer_self for q in c.objects]
        ]
        # FIXME: the coords are wrong
        yy, xx = np.unravel_index(D, (W.width, W.height))
        X2 = np.c_[xx, yy]
        # get corner points for wall
        m_min = np.min(X2, axis=0)
        m_max = np.max(X2, axis=0)
        debug("mmin=%s,mmax=%s", m_min, m_max)
        cc = (
            np.array(
                [
                    [m_max[0] + 1, m_max[1] + 1],  # BL
                    [m_max[0] + 1, m_min[1] - 1],  # BR
                    [m_min[0] - 1, m_max[1] + 1],  # TL
                    [m_min[0] - 1, m_min[1] - 1],  # TR
                ]
            )
            * 256
        )

        OF = ObjectFactory(self.sm)
        cc2 = (cc / 256).astype(np.int64)
        cc = np.c_[cc, np.ones((cc.shape[0], 1)) * o_conyard.coordinates.z]
        # wall MCV
        debug("place coords=%s", cc / 256)
        for c in cc:
            await self.produce_and_place(
                tc_wall, c, strategy=PlaceStrategy.ABSOLUTE
            )

        await asyncio.sleep(1.0)
        # get player objs
        debug(
            "objs=%s",
            [
                OF.create_object(o).tc()
                for o in self.house_objects(self.current_player())
            ],
        )
        # get all wall cells
        W = await self.map_data()
        D = [
            c.index
            for c in W.cells
            if c.overlay_type_index == tc_wall_ol.array_index
        ]
        debug("wall cells %s", D)
        # FIXME: the coords are wrong
        yy, xx = np.unravel_index(D, (W.width, W.height))
        X2 = np.c_[xx, yy]
        # wall_cells = []
        # # sell all but two walls
        # objs_wall = [
        #     q
        #     for q in (
        #         OF.create_object(o)
        #         for o in self.house_objects(self.current_player())
        #     )
        #     if re.match(r"Soviet\s+Wall", q.tc().name)
        # ]
        # debug("walls:%s", objs_wall)
        cc3 = (X2 * 256).astype(np.int64)
        cc = np.c_[cc3, np.zeros((cc3.shape[0], 1))]
        # sell walls
        debug("wall cells: %s", X2)
        for c in cc[:-3, ...]:
            await U.sell_walls(array2coord(c))

        # get cells that contain player objs
        # yy, xx = np.unravel_index(D, (M.width, M.height))
        # X2 = np.c_[xx, yy]

        await asyncio.sleep(2)
        await U.sell(objects=[o_conyard])
        await asyncio.sleep(2)

        # order connies to attack tesla reactor
        connies = list(self.sm.get_objects(tc_conscript))
        o_tesla = OF.create_object(self.sm.get_object(b_tesla))

        r = await U.unit_order(
            objects=connies,
            action=ra2yr.UNIT_ACTION_ATTACK,
            target_object=o_tesla.o,
        )

        # wait until tesla health drops under certain value, then order engineer to repair it
        await M.wait_state(
            lambda: o_tesla.update()
            and o_tesla.o.health < o_tesla.tc().strength
        )

        r = await U.unit_order(
            objects=self.sm.get_objects(tc_engi),
            action=ra2yr.UNIT_ACTION_CAPTURE,
            target_object=o_tesla.o,
        )

        await U.wait_game_to_exit()

        await self.verify_recording()


if __name__ == "__main__":
    unittest.main()
