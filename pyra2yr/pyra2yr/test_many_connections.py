import asyncio
import datetime
import gzip
import logging
import os
import re
from typing import List

from ra2yrproto import commands_builtin, commands_yr, ra2yr

from pyra2yr.manager import Manager, read_protobuf_messages
from pyra2yr.network import log_exceptions

logging.basicConfig(level=logging.DEBUG)
debug = logging.debug
info = logging.info


async def mcv_sell(app=None):
    n_managers = 2
    managers = [Manager(poll_frequency=30) for i in range(n_managers)]
    for m in managers:
        m.start()

    # wait until all managers are ingame
    for i, m in enumerate(managers):
        await m.wait_state(lambda: m.state.stage == ra2yr.STAGE_INGAME)
        debug("ingame=%d", i)

    # disconnect some
    M0 = managers.pop(0)
    await M0.stop()

    # connect new manager
    M1 = Manager(poll_frequency=30)
    M1.start()
    managers.append(M1)

    await asyncio.sleep(1)
    # disconnect all
    for m in managers:
        await m.stop()
    await asyncio.sleep(1)


async def test_sell_mcv(host: str):
    t = asyncio.create_task(log_exceptions(mcv_sell()))
    await t


if __name__ == "__main__":
    asyncio.run(test_sell_mcv("0.0.0.0"))
