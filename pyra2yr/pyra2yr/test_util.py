import gzip
import logging as lg
import unittest
from typing import Iterator, Type

import numpy as np
from ra2yrproto import commands_yr, ra2yr

from pyra2yr.manager import Manager, ManagerUtil
from pyra2yr.util import (
    StateUtil,
    array2coord,
    coord2array,
    read_protobuf_messages,
)


def verify_recording(path: str):
    n_deploy = 0
    last_frame = 0
    with gzip.open(path, "rb") as f:
        m = read_protobuf_messages(f)

        # get type classes
        m0 = next(m)
        S = StateUtil(m0.object_types)
        S.set_state(m0)
        last_frame = S.state.current_frame
        for _, m0 in enumerate(m):
            S.set_state(m0)
            for u in S.get_units("player_0"):
                if u.deployed or u.deploying:
                    n_deploy += 1
            cur_frame = S.state.current_frame
            assert (
                cur_frame - last_frame == 1
            ), f"cur,last={cur_frame},{last_frame}"
            last_frame = S.state.current_frame
    assert last_frame > 0
    assert n_deploy > 0


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

    def house_objects(self, h: ra2yr.House) -> Iterator[ra2yr.Object]:
        return (o for o in self.sm.s.objects if o.pointer_house == h.self)

    def current_player(self) -> ra2yr.House:
        try:
            return next(p for p in self.sm.s.houses if p.current_player)
        except StopIteration:
            lg.error(
                "fatal, invalid current player. houses: %s",
                self.sm.s.houses,
            )
            raise

    def my_buildings(self) -> Iterator[ra2yr.Object]:
        #
        return (
            o
            for o in self.house_objects(self.current_player())
            if o.object_type == ra2yr.ABSTRACT_TYPE_BUILDING
        )

    def get_production(
        self, h: ra2yr.House = None
    ) -> Iterator[tuple[ra2yr.Object, ra2yr.Factory]]:
        if not h:
            return (
                (self.sm.get_object(o.object), o) for o in self.sm.s.factories
            )
        return (
            (self.sm.get_object(o.object), o)
            for o in self.sm.s.factories
            if o.owner == h.self
        )

    async def get_place_locations(
        self, coords: np.array, o: ra2yr.Object, rx: int, ry: int
    ) -> np.array:
        xx = np.arange(rx) - int(rx / 2)
        yy = np.arange(ry) - int(ry / 2)
        if coords.size < 3:
            coords = np.append(coords, 0)
        grid = (
            np.transpose([np.tile(xx, yy.shape), np.repeat(yy, xx.shape)]) * 256
        )
        grid = np.c_[grid, np.zeros((grid.shape[0], 1))] + coords
        # result = np.c_[result, np.zeros((result.shape[0], 1))]
        res = await self.M.M.place_query(
            type_class=o.pointer_technotypeclass,
            house_class=o.pointer_house,
            coordinates=[array2coord(x) for x in grid],
        )
        return np.array([coord2array(x) for x in res.coordinates])

    def get_factories(self, h: ra2yr.House) -> Iterator[ra2yr.Factory]:
        return (x for x in self.sm.s.factories if x.owner == h.self)
