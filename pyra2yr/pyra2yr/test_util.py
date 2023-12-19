import asyncio
import logging as lg
import unittest
from typing import Type

import numpy as np
from ra2yrproto import commands_yr, core, ra2yr

from pyra2yr.manager import Manager, ManagerUtil, PlaceStrategy
from pyra2yr.state_objects import FactoryEntry, MapData, ObjectEntry
from pyra2yr.util import array2coord, coord2array


async def check_config(U: ManagerUtil = None):
    # Get config
    cmd_1 = await U.inspect_configuration()
    cfg1 = cmd_1.config
    cfg_ex = commands_yr.Configuration()
    cfg_ex.CopyFrom(cfg1)
    cfg_ex.debug_log = True
    cfg_ex.parse_map_data_interval = 1
    assert cfg_ex == cfg1

    # Try changing some settings
    cfg_diff = commands_yr.Configuration(parse_map_data_interval=4)
    cfg2_ex = commands_yr.Configuration()
    cfg2_ex.CopyFrom(cfg1)
    cfg2_ex.MergeFrom(cfg_diff)
    cmd_2 = await U.inspect_configuration(config=cfg_diff)
    cfg2 = cmd_2.config
    assert cfg2 == cfg2_ex


class MyManager(Manager):
    async def get_place_locations(
        self, coords: np.array, o: ra2yr.Object, rx: int, ry: int
    ) -> np.array:
        """Return coordinates where a building can be placed.

        Parameters
        ----------
        coords : np.array
            Center point
        o : ra2yr.Object
            The ready building
        rx : int
            Query x radius
        ry : int
            Query y radius

        Returns
        -------
        np.array
            Result coordinates.
        """
        xx = np.arange(rx) - int(rx / 2)
        yy = np.arange(ry) - int(ry / 2)
        if coords.size < 3:
            coords = np.append(coords, 0)
        grid = (
            np.transpose([np.tile(xx, yy.shape), np.repeat(yy, xx.shape)]) * 256
        )
        grid = np.c_[grid, np.zeros((grid.shape[0], 1))] + coords
        res = await self.M.place_query(
            type_class=o.pointer_technotypeclass,
            house_class=o.pointer_house,
            coordinates=[array2coord(x) for x in grid],
        )
        return np.array([coord2array(x) for x in res.coordinates])

    def get_unique_tc(self, pattern) -> ra2yr.ObjectTypeClass:
        tc = list(self.state.query_type_class(p=pattern))
        if len(tc) != 1:
            raise RuntimeError(f"Non unique TypeClass: {pattern}: {tc}")
        return tc[0]

    async def begin_production(self, t: ra2yr.ObjectTypeClass) -> FactoryEntry:
        # TODO: Check for error
        await self.M.start_production(t)
        frame = self.state.s.current_frame

        # Wait until corresponding type is being produced
        await self.wait_state(
            lambda: any(
                f
                for f in self.state.query_factories(
                    h=self.state.current_player(), t=t
                )
            )
            or self.state.s.current_frame - frame >= 300
        )
        try:
            # Get the object
            return next(
                self.state.query_factories(h=self.state.current_player(), t=t)
            )
        except StopIteration as e:
            raise RuntimeError(f"Failed to start production of {t}") from e

    async def produce_unit(self, t: ra2yr.ObjectTypeClass | str) -> ObjectEntry:
        if isinstance(t, str):
            t = self.get_unique_tc(t)

        fac = await self.begin_production(t)

        # Wait until done
        # TODO: check for cancellation
        await self.wait_state(
            lambda: fac.object.get().current_mission == ra2yr.Mission_Guard
        )
        return fac.object

    async def produce_and_place(
        self,
        t: ra2yr.ObjectTypeClass,
        coords,
        strategy: PlaceStrategy = PlaceStrategy.FARTHEST,
    ) -> ObjectEntry:
        U = self.M
        fac = await self.begin_production(t)
        obj = fac.object

        # wait until done
        await self.wait_state(lambda: fac.get().completed)

        if strategy == PlaceStrategy.FARTHEST:
            place_locations = await self.get_place_locations(
                coords,
                obj.get(),
                15,
                15,
            )

            # get cell closest away and place
            dists = np.sqrt(np.sum((place_locations - coords) ** 2, axis=1))
            coords = place_locations[np.argsort(dists)[-1]]
        r = await U.place_building(
            building=obj.get(), coordinates=array2coord(coords)
        )
        if r.result_code != core.ResponseCode.OK:
            raise RuntimeError(f"place failed: {r.error_message}")
        # wait until building has been placed
        await self.wait_state(
            lambda: obj.invalid()
            or fac.invalid()
            and obj.get().current_mission
            not in (ra2yr.Mission_None, ra2yr.Mission_Construction)
        )
        return obj

    async def get_map_data(self) -> MapData:
        W = await self.M.map_data()
        return MapData(W)


class BaseGameTest(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.poll_frequency = 30
        self.fetch_state_timeout = 10.0
        self.M = self.get_manager()
        self.sm = self.M.state
        self.M.start()

    async def asyncTearDown(self):
        await self.M.stop()

    async def check_record_output_defined(self):
        # Get config
        cmd_1 = await self.M.M.inspect_configuration()
        cfg1 = cmd_1.config
        self.assertNotEqual(
            cfg1.record_filename,
            "",
            "Record output wasn't set. Make sure RA2YRCPP_RECORD_PATH environment variable is set.",
        )

    def get_manager_class(self) -> Type[Manager]:
        return Manager

    def get_manager(self) -> Manager:
        return self.get_manager_class()(
            poll_frequency=self.poll_frequency,
            fetch_state_timeout=self.fetch_state_timeout,
        )
