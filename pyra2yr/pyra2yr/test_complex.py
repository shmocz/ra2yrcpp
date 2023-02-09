import asyncio
import logging
import re
from typing import List

from ra2yrproto import ra2yr

from pyra2yr.manager import Manager, ManagerUtil
from pyra2yr.network import log_exceptions
from pyra2yr.util import StateUtil, coord2tuple, pdist

logging.basicConfig(level=logging.DEBUG)
debug = logging.debug
info = logging.info


class MyManager(Manager):
    def __init__(self, *args, **kwargs):
        self.ttc_map = {}
        self.tc_tesla = None
        self.U = StateUtil(None)
        self.M = ManagerUtil(self)
        self.ready_buildings = set()
        self.pending_place = set()
        super().__init__(*args, **kwargs)

    @property
    def started(self):
        return (
            self.state.stage == ra2yr.STAGE_INGAME
            and self.state.current_frame > 1
        )

    def get_object(self, p: int) -> ra2yr.Object:
        return next(o for o in self.state.objects if o.pointer_self == p)

    def my_units(self):
        self.U.set_state(self.state)
        return self.U.get_units(self.U.get_self().name)

    def get_ttcs(self, n: str):
        return (t for t in self.type_classes if re.search(n, t.name))

    def tesla_reactors(self):
        return self.get_units(r"Tesla\s+Reactor")

    # produce building if possible
    async def try_produce(self, index: int):
        objs = [o.object for o in self.production()]
        ttc = [t for t in self.type_classes if t.array_index == index][0]
        my = self.my_units()
        my_ids = set(
            self.ttc_map[u.pointer_technotypeclass].array_index for u in my
        )

        preq = set(ttc.prerequisites)
        myids = set(my_ids)
        # prerequisites met?
        if not preq == myids:
            return

        # already producing?
        if [o for o in objs if self.ttc_map[o].array_index == index]:
            return

        # issue produce event
        h = self.U.get_self()
        await self.M.produce_building(house_index=h.array_index, heap_id=index)

    def get_units(self, pt_name):
        return [
            u
            for u in self.my_units()
            if re.search(pt_name, self.ttc_map[u.pointer_technotypeclass].name)
        ]

    def conyards(self):
        return self.get_units(r"Yard")

    def mcvs(self):
        return self.get_units(r"Vehicle")

    def production(self):
        return self.U.get_production(self.U.get_self().name)

    def done_production(self) -> List[ra2yr.Factory]:
        return [o for o in self.production() if o.progress_timer == 54]

    async def try_place(self):
        # if previous place operation pending, dont do anything
        pass

    def clear_pending_place(self):
        done = {o.object for o in self.done_production()}
        uu = [
            u
            for u in self.my_units()
            if u.pointer_self in self.pending_place
            and u.pointer_self not in done
        ]
        for u in uu:
            self.pending_place.remove(u.pointer_self)

    def setup_ttc_map(self):
        if not self.ttc_map:
            self.ttc_map = {t.pointer_self: t for t in self.type_classes}

    async def game_step(self, s: ra2yr.GameState):
        if not self.type_classes:
            debug("no type classes yet")
            return

        self.setup_ttc_map()

        if not self.conyards():
            mcv = self.mcvs()[0]
            if not mcv.deploying or mcv.deployed:
                # FIXME: make a method that properly emulates actual actions (select and then deploy)
                await self.M.deploy(mcv.pointer_self)
            return

        if not self.tesla_reactors():
            await self.try_produce(
                next(self.get_ttcs(r"Tesla\s+Reactor")).array_index
            )

        self.clear_pending_place()
        done = self.done_production()
        if done and not self.pending_place:
            o = self.get_object(done[0].object)
            b = self.conyards()[0]
            coords = coord2tuple(b.coordinates)
            res = await self.M.get_place_locations(
                coords,
                next(self.get_ttcs(r"Tesla\s+Reactor")).pointer_self,
                self.U.get_self().self,
                5,
                5,
            )
            # get cell furthest away and place
            dists = [
                (i, pdist(coords, coord2tuple(c)))
                for i, c in enumerate(res.result.coordinates)
            ]
            debug("dists=%s, pending=%s", dists, self.pending_place)
            i0, _ = sorted(dists, key=lambda x: x[1], reverse=True)[0]

            await self.M.place_building(
                heap_id=self.ttc_map[o.pointer_technotypeclass].array_index,
                location=res.result.coordinates[i0],
            )
            self.pending_place.add(done[0].object)

    async def step(self, s: ra2yr.GameState):
        if self.started:
            await self.game_step(s)


async def mcv_sell(app=None):
    M = MyManager(poll_frequency=30)
    M.start()
    await M.wait_state(
        lambda: M.state.stage == ra2yr.STAGE_INGAME
        and M.state.current_frame > 1
    )
    debug("wait game to exit")
    await M.wait_state(lambda: M.state.stage == ra2yr.STAGE_EXIT_GAME)


async def test_sell_mcv(host: str):
    t = asyncio.create_task(log_exceptions(mcv_sell()))
    await t


if __name__ == "__main__":
    asyncio.run(test_sell_mcv("0.0.0.0"))
