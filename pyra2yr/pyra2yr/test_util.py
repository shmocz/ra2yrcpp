import gzip

from pyra2yr.util import StateUtil, read_protobuf_messages
from pyra2yr.manager import ManagerUtil, Manager
import unittest
from ra2yrproto import commands_yr
from typing import Type


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
            f"Record output wasn't set. Make sure RA2YRCPP_RECORD_PATH environment variable is set.",
        )

    def get_manager_class(self) -> Type[Manager]:
        return Manager

    def get_manager(self) -> Manager:
        return self.get_manager_class()(
            poll_frequency=self.poll_frequency,
            fetch_state_timeout=self.fetch_state_timeout,
        )
