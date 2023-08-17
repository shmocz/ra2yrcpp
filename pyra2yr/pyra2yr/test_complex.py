import asyncio
import cProfile
import logging as lg
import random
import re
from enum import Enum
from functools import cached_property
from typing import List, Tuple, Union

import numpy as np
from ra2yrproto import ra2yr

from pyra2yr.game_tasks import AsyncGameTask, DeployTask, PlaceTask, ProduceTask
from pyra2yr.manager import (
    ActionTracker,
    CachedEntry,
    Manager,
    ManagerUtil,
    NumpyMapData,
)
from pyra2yr.network import logged_task
from pyra2yr.util import (
    StateUtil,
    coord2tuple,
    equals,
    ind2sub,
    pdist,
    setup_logging,
    sub2ind,
    tuple2coord,
    PrerequisiteCheck,
    check_preq_list,
    check_stolen_tech,
)

# FIXME: parallel naval + tank build

PT_SCOUT = r"^Attack.*Dog"
PT_ENGI = r"Soviet Engineer"
PT_DERRICK = r"Derrick"
PT_CONNIE = r"Conscript"


def filter_ttc(
    T: List[ra2yr.ObjectTypeClass],
    x: Union[str, ra2yr.Object, Tuple[ra2yr.AbstractType, int]],
):
    """_summary_

    Parameters
    ----------
    T : List[ra2yr.ObjectTypeClass]
        _description_
    x : Union[str, ra2yr.Object, ra2yr.AbstractType]
        One of:
        str: regex pattern to match the name against
        ra2yr.Object: TTC of given object
        ra2yr.AbstractType: pair of (rtti_id, array_index)

    Returns
    -------
    _type_
        _description_
    """
    if isinstance(x, str):
        return (t for t in T if re.search(x, t.name))
    if isinstance(x, ra2yr.Object):
        return (t for t in T if t.pointer_self == x.pointer_technotypeclass)
    if isinstance(x, tuple):
        return (t for t in T if t.type == x[0] and t.array_index == x[1])
    return None


class PlaceStrategy(Enum):
    RANDOM = 0
    FARTHEST = 1


class MyManager(Manager):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.ttc_map = {}
        self.U = StateUtil(None)
        self.M = ManagerUtil(self)
        self._map_data: ra2yr.MapDataSoA = None
        self._frame_map_data = 0
        self._num_scout_units = 3
        self.place_strategy = PlaceStrategy.RANDOM
        self.pending_tasks: List[AsyncGameTask] = []
        self._naval_allowed = None
        self._objects_numpy = None
        self._objects_numpy_updated = 0
        self._ttc_numpy = CachedEntry(self.state)
        self.pr = cProfile.Profile()
        self.pr_on = False
        self._produceable = CachedEntry(self.state)
        self.profile_duration = 20 * 60
        self.map_data_numpy = None
        self._engi_ttc = None
        self._at = ActionTracker(self.state)
        self._at.add_tracker("scouter", 20)
        self._at.add_tracker("capture", 30)
        self._at.add_tracker("produce", 15)
        self._at.add_tracker("place", 20)
        random.seed(1234)

    # TODO: save previous state and check if anything changed
    def get_produceable(self) -> List[ra2yr.ObjectTypeClass]:
        my_buildings = self.my_buildings()
        my_production = self.my_production()
        prod_objs = set(p.pointer_technotypeclass for p in my_production)
        myids = set(
            t.array_index
            for t in my_buildings
            if t.pointer_self not in prod_objs
        )

        all_preqs = []
        H = self.U.get_self()
        for t in (x for x in self.theoretically_produceable):
            if self.can_produce_ttc(t, H, myids):
                all_preqs.append(t)
        return all_preqs

    @cached_property
    def theoretically_produceable(self) -> List[ra2yr.ObjectTypeClass]:
        return self.get_theoretically_produceable()

    @property
    def produceable(self):
        if self._produceable.should_update():
            self._produceable.update(self.get_produceable())
        return self._produceable.get()

    @property
    def objects_numpy(self) -> np.array:
        if (
            self._objects_numpy is None
            or self._objects_numpy_updated < self.state.current_frame
        ):
            self._objects_numpy = np.array(
                [
                    (
                        o.pointer_house,
                        o.pointer_self,
                        o.pointer_technotypeclass,
                        o.object_type,
                        o.current_mission,
                        o.coordinates.x,
                        o.coordinates.y,
                        o.coordinates.z,
                        o.destination.x,
                        o.destination.y,
                        o.destination.z,
                    )
                    for o in self.state.objects
                ]
            )
            self._objects_numpy_updated = self.state.current_frame
        return self._objects_numpy

    @property
    def ttc_numpy(self) -> np.array:
        if self.type_classes and self._ttc_numpy.value is None:
            self._ttc_numpy.update(
                np.array(
                    [
                        (
                            p.cost,
                            p.array_index,
                            p.pointer_self,
                            p.type,
                            p.wall,
                            p.naval,
                        )
                        for p in self.type_classes
                    ]
                )
            )

        return self._ttc_numpy.get()

    def naval_allowed(self, map_data: NumpyMapData) -> bool:
        """If we can find at least a 3x3 location of water, then assume we can build navy."""

        if self._naval_allowed is None:
            self._naval_allowed = False
            ww, hh = (map_data.width, map_data.height)
            xx = map_data.dm[..., 0].flatten()
            window = [(i, j) for i in range(3) for j in range(3)]
            for i in range(xx.size):
                y, x = ind2sub(i, ww, hh)
                # check neighborhood
                inds = [sub2ind(y + w[0], x + w[1], ww, hh) for w in window]
                if all(xx[i] == ra2yr.LAND_TYPE_Water for i in inds):
                    self._naval_allowed = True
                    break

        return self._naval_allowed

    def get_task(self, x):
        l = list(t for t in self.pending_tasks if t.equals(x))
        if not l:
            return None
        return l[0]

    @property
    def started(self):
        return (
            self.state.stage == ra2yr.STAGE_INGAME
            and self.state.current_frame > 1
        )

    async def map_data(self) -> ra2yr.MapDataSoA:
        if (
            not self._map_data
            or self._frame_map_data < self.state.current_frame
        ):
            res = await self.M.read_value(map_data_soa=ra2yr.MapDataSoA())
            map_data = res.data.map_data_soa
            self._map_data = map_data
            self._frame_map_data = self.state.current_frame
            if not self.map_data_numpy:
                self.map_data_numpy = NumpyMapData(self._map_data)
            self.map_data_numpy.update(self._map_data)
        return self._map_data

    def get_object(self, p: int) -> ra2yr.Object:
        return next(o for o in self.U.state.objects if o.pointer_self == p)

    def find_objects(self, p: Union[str, int]) -> ra2yr.Object:
        if isinstance(p, str):

            def flt(o):
                return re.search(
                    p, self.ttc_map[o.pointer_technotypeclass].name
                )

        elif isinstance(p, int):

            def flt(o):
                return o.pointer_self == p

        return (o for o in self.state.objects if flt(o))

    def preq_check(
        self,
        ttc: ra2yr.ObjectTypeClass,
        h: ra2yr.House,
        myids,
    ) -> PrerequisiteCheck:
        res = PrerequisiteCheck.OK
        res |= check_preq_list(ttc, myids, self.prerequisite_map)

        if check_stolen_tech(ttc, h):
            res |= PrerequisiteCheck.NO_STOLEN_TECH
        # TODO: redundant double checks
        if ttc.tech_level < 0 or self.state.tech_level < ttc.tech_level:
            res |= PrerequisiteCheck.BAD_TECH_LEVEL
        if (
            ttc.forbidden_houses != -1
            and (1 << h.type_array_index) & ttc.forbidden_houses
        ):
            res |= PrerequisiteCheck.FORBIDDEN_HOUSE
        if (
            ttc.build_limit > 0
            and np.sum(
                (self.objects_numpy[:, 0] == h.self)
                & (self.objects_numpy[:, 2] == ttc.pointer_self)
            )
            >= ttc.build_limit
        ):
            res |= PrerequisiteCheck.BUILD_LIMIT_REACHED

        return res

    def can_produce_ttc(
        self,
        ttc: ra2yr.ObjectTypeClass,
        H: ra2yr.House = None,
        myids=None,
        verbose=False,
    ) -> bool:
        if (
            ttc.forbidden_houses != -1
            and (1 << H.type_array_index) & ttc.forbidden_houses
        ):
            return False
        if not (
            ((1 << H.type_array_index) & ttc.required_houses)
            and ((1 << H.type_array_index) & ttc.owner_flags)
        ):
            return False

        res = self.preq_check(ttc, H, myids)
        if verbose:
            lg.debug("check result=%s", res)
        return res == PrerequisiteCheck.OK

    async def try_move(self, o: ra2yr.Object, destination: ra2yr.Coordinates):
        key1 = (o.pointer_self, o.current_mission, coord2tuple(o.destination))
        key2 = (o.pointer_self, o.current_mission, coord2tuple(destination))
        if key1 != key2:
            await self.M.move(
                object_addresses=[o.pointer_self], coordinates=destination
            )

    # NB. gives unique values!
    def my_buildings(self) -> List[ra2yr.ObjectTypeClass]:
        h = self.U.get_self()
        O = self.objects_numpy
        T = self.ttc_numpy
        m_my_objs = O[:, 0] == h.self
        ttcs_ix = np.nonzero(
            np.isin(T[:, 2], np.unique(O[m_my_objs, 2]))
            & (T[:, 3] == ra2yr.ABSTRACT_TYPE_BUILDINGTYPE)
        )[0]
        return [self.type_classes[i] for i in ttcs_ix]

    def my_production(self):
        O = self.objects_numpy
        my_production_ids = np.array([p.object for p in self.production()])
        my_production_objs_ix = np.nonzero(np.isin(O[:, 1], my_production_ids))
        return [self.state.objects[i] for i in my_production_objs_ix[0]]

    def get_theoretically_produceable(self) -> List[ra2yr.ObjectTypeClass]:
        my_buildings = self.my_buildings()
        my_production = self.my_production()

        P = PrerequisiteCheck
        all_preqs = []
        h = self.U.get_self()
        prod_objs = set(p.pointer_technotypeclass for p in my_production)
        myids = set(
            t.array_index
            for t in my_buildings
            if t.pointer_self not in prod_objs
        )

        for t in (x for x in self.type_classes):
            r = self.preq_check(t, h, myids)
            if (
                not (
                    P.EMPTY_PREREQUISITES
                    | P.INVALID_PREQ_GROUP
                    | P.BAD_TECH_LEVEL
                    | P.FORBIDDEN_HOUSE
                )
                & r
            ):
                all_preqs.append(t)
        lg.debug("theoretically produceable: %d", len(all_preqs))
        lg.debug("naval objs: %s", [t.name for t in all_preqs if t.naval])

        return all_preqs

    # produce item if possible
    async def try_produce(
        self, index: int, rtti_id: int = ra2yr.ABSTRACT_TYPE_BUILDINGTYPE
    ):
        h = self.U.get_self()

        # prerequisites met?
        # TODO: broken? - array indices can overlap
        if not index in [
            p.array_index for p in self.produceable if p.type == rtti_id
        ]:
            return

        target = [
            t
            for t in self.type_classes
            if t.type == rtti_id and t.array_index == index
        ][0]

        if target.naval:
            await self.map_data()
            naval_allowed = self.naval_allowed(self.map_data_numpy)
            if not naval_allowed:
                lg.debug("naval not allowed in this map")
                return

        t = ProduceTask(self.state, target, h, self.type_classes)
        if t.already_producing():
            return
        if self.get_task(t):
            return
        self.pending_tasks.append(t)

        # TODO: verify result
        await self.M.produce(
            rtti_id=rtti_id,
            heap_id=index,
            is_naval=target.naval,
        )

    async def try_produce_ttc(self, t: ra2yr.ObjectTypeClass):
        await self.try_produce(index=t.array_index, rtti_id=t.type)

    def get_units(self, pt_name):
        my_units = [u for u in self.U.get_units(self.U.get_self().name)]
        return [
            u
            for u in my_units
            if re.search(pt_name, self.ttc_map[u.pointer_technotypeclass].name)
        ]

    def conyards(self):
        return self.get_units(r"Yard")

    def mcvs(self):
        return self.get_units(r"Vehicle")

    def scout_units(self) -> List[ra2yr.Object]:
        return [
            u for u in self.get_units(PT_SCOUT) if u.HasField("coordinates")
        ]

    def production(self):
        return self.U.get_production(self.U.get_self().name)

    def setup_ttc_map(self):
        if not self.ttc_map:
            self.ttc_map = {t.pointer_self: t for t in self.type_classes}

    async def try_deploy(self, o: ra2yr.Object):
        t = DeployTask(self.state, o)
        if not self.get_task(t):
            self.pending_tasks.append(t)
            await self.M.deploy(o.pointer_self)

    async def deploy_main_conyard(self):
        # TODO: make a method that properly emulates actual actions (select and then deploy)
        await self.try_deploy(self.mcvs()[0])

    async def choose_building_to_produce(self) -> ra2yr.ObjectTypeClass:
        if not self._at.proceed("produce"):
            return
        production = [
            self.ttc_map[o.pointer_technotypeclass]
            for o in self.my_production()
        ]

        await self.map_data()
        naval_ok = self.naval_allowed(self.map_data_numpy)

        # get all buildable buildings except for wall and potentially naval
        produceable = [
            b
            for b in self.produceable
            if b.type == ra2yr.ABSTRACT_TYPE_BUILDINGTYPE
            and not b.wall
            and not (b.naval and not naval_ok)
        ]

        keys = {(ttc.build_category, ttc.type) for ttc in production}
        buildable = [
            b for b in produceable if (b.build_category, b.type) not in keys
        ]

        if not buildable:
            return None

        # check that we have enough power for everything, if not build tesla reactor or NR
        me = self.U.get_self()

        power_difference = me.power_output - me.power_drain
        if power_difference < 0:
            pp = [b for b in buildable if b.power_bonus > 0]
            if pp:
                return sorted(pp, key=lambda x: -x.power_bonus)[0]

        # pick a building we haven't got yet
        # sort buildings by build count
        O = self.objects_numpy
        my_objs = O[O[:, 0] == me.self, :]

        buildable_counts = [
            (i, np.sum(my_objs[:, 2] == b.pointer_self))
            for i, b in enumerate(buildable)
        ]
        bb = sorted(
            (b for b in buildable_counts if b[1] < 2), key=lambda x: x[1]
        )
        if bb:
            return buildable[bb[0][0]]
        return None

    # TODO: decorator for proceed
    async def place_buildings(self):
        """Place all pending buildings

        Issues a PLACE event for each building that is ready and can be deployed to suitable location.
        In practice, the place might be delayed (e.g. due to lag). To avoid spamming extra requests during this "lag" phase,
        each request is put into pending queue and removed once completed, failed or not
        """
        if not self._at.proceed("place"):
            return
        done = [
            o
            for o in self.production()
            if o.progress_timer == 54
            if self.ttc_map[
                self.get_object(o.object).pointer_technotypeclass
            ].type
            == ra2yr.ABSTRACT_TYPE_BUILDINGTYPE
        ]

        if done:
            o = self.get_object(done[0].object)
            t = PlaceTask(self.state, o)
            if self.get_task(t):
                return
            coords = coord2tuple(self.conyards()[0].coordinates)
            res = await self.M.get_place_locations(
                coords,
                self.ttc_map[o.pointer_technotypeclass].pointer_self,
                self.U.get_self().self,
                30,
                30,
            )
            res_coords_l = list(enumerate(res.coordinates))

            if not res_coords_l:
                lg.error(
                    "no valid place coords for %s",
                    self.ttc_map[o.pointer_technotypeclass].name,
                )
                return
            # get cell furthest away and place
            if self.place_strategy == PlaceStrategy.FARTHEST:
                dists = [
                    (i, pdist(coords, coord2tuple(c))) for i, c in res_coords_l
                ]
                # pick random place location
                i0, _ = sorted(dists, key=lambda x: x[1], reverse=True)[0]
            else:
                i0 = random.choice(res_coords_l)[0]

            await self.M.place_building(
                heap_id=self.ttc_map[o.pointer_technotypeclass].array_index,
                location=res_coords_l[i0][1],
            )
            self.pending_tasks.append(t)

    def get_shroud_targets(self, shrouded_cells, D):
        SU = self.scout_units()
        if not SU:
            return []
        # choose unshrouded cell closest to mcv and the scouter
        coords = np.array(coord2tuple(self.conyards()[0].coordinates)).reshape(
            (1, -1)
        )

        yy, xx = np.unravel_index(shrouded_cells, (D.map_width, D.map_height))
        X2 = np.c_[xx, yy]
        # shrouded cells
        P1 = (
            (
                np.c_[
                    X2,
                    np.zeros((shrouded_cells.size, 1)),
                ]
            )
            + np.array([0.5, 0.5, 0])
        ) * 256

        # scouters coordinates
        P2 = np.reshape(
            np.array([coord2tuple(u.coordinates) for u in SU]), (len(SU), 1, -1)
        )
        # distance to MCV
        D1 = np.sqrt(np.sum((P1 - coords) ** 2, axis=1))
        # distance to scouters
        D2 = np.sqrt(np.sum((P1 - P2) ** 2, axis=2))
        # find minimums to combined distance and assign to scouters
        imin = np.argsort(D1 + D2, axis=1)

        targets = []
        for i in range(imin.shape[0]):
            targets.append(
                next(
                    imin[i, j]
                    for j in range(imin.shape[1])
                    if imin[i, j] not in targets
                )
            )

        return targets

    async def scouter_actions(self):
        if not self._at.proceed("scouter"):
            return
        # just move to random unshrouded spot
        # if moving, and destination is unshrouded, choose new destination
        SU = self.scout_units()
        D = await self.map_data()
        N = self.map_data_numpy
        S = (N.dm[..., N.keys.index("shrouded")] > 0) & (
            N.dm[..., N.keys.index("passability")] == 0
        )
        shrouded_cells = np.flatnonzero(S)

        if shrouded_cells.size == 0:
            return

        targets = self.get_shroud_targets(shrouded_cells, D)

        for su, t in zip(SU, targets):
            sub = ind2sub(shrouded_cells[t], D.map_width, D.map_height) + (0,)
            await self.try_move(
                o=su, destination=tuple2coord(tuple(256 * x for x in sub))
            )
        return

    async def explore_shrouded(self):
        if len(self.scout_units()) < self._num_scout_units:
            t = list(filter_ttc(self.produceable, PT_SCOUT))
            if t:
                await self.try_produce_ttc(t[0])
        await self.scouter_actions()

    async def capture_buildings(self):
        if not self._at.proceed("capture"):
            return
        derrick_ttc = next(filter_ttc(self.type_classes, PT_DERRICK))
        O = self.objects_numpy
        my_objs = O[:, 0] == self.U.get_self().self
        derricks = O[
            ~my_objs & (O[:, 2] == derrick_ttc.pointer_self),
            :,
        ]
        # dont do anything if nothing to capture
        if derricks.size == 0:
            return

        # build engi if needed
        if not self._engi_ttc:
            self._engi_ttc = list(filter_ttc(self.produceable, PT_ENGI))
        engi_ttc = next(filter_ttc(self.type_classes, PT_ENGI))
        engis = O[my_objs & (O[:, 2] == engi_ttc.pointer_self), :]
        if engis.shape[0] < derricks.shape[0] and self._engi_ttc:
            await self.try_produce_ttc(self._engi_ttc[0])

        # get missions
        engis = engis[:, [1, 4, 8, 9, 10]]
        M = np.c_[engis[:, :2], np.floor(engis[:, 2:] / 256)]
        D = derricks[:, [1, 4, 5, 6, 7]]

        # TODO: filter capture mission!
        derricks_non_targeted = D[
            ~(np.floor((D[:, 2:]) / 256)[:, None] == M[:, 2:]).all(-1).any(-1),
            :,
        ]

        E_noncapturing = engis[engis[:, 1] != ra2yr.Mission_Capture, :]

        for i in range(
            min(E_noncapturing.shape[0], derricks_non_targeted.shape[0])
        ):
            d = derricks_non_targeted[i, :]
            coords = tuple2coord(list(d[2:]))
            await self.M.capture(
                object_addresses=[E_noncapturing[i, 0]],
                coordinates=coords,
                target_object=d[0],
            )

    async def clear_completed_tasks(self):
        pending = []
        for i, t in enumerate(self.pending_tasks):
            if not await t.is_completed():
                pending.append(i)
        self.pending_tasks = [self.pending_tasks[i] for i in pending]

    async def produce_attackers(
        self,
        max_count=5,
        mask=ra2yr.ABSTRACT_TYPE_INFANTRYTYPE | ra2yr.ABSTRACT_TYPE_UNITTYPE,
    ):
        X = self.objects_numpy

        U = np.array(
            [
                (
                    p.cost,
                    p.array_index,
                    p.pointer_self,
                    p.type,
                    p.wall,
                    p.naval,
                )
                for p in self.produceable
            ]
        )
        U = U[
            (U[:, 3] & mask) > 0,
            :,
        ]
        counts = [np.sum(X[:, 2] == U[i, 2]) for i in range(U.shape[0])]
        # pick the one with fewest
        I = np.argsort(counts)
        if I.size < 1 or counts[I[0]] >= max_count:
            return

        ttc = self.ttc_map[U[I[0], 2]]
        await self.try_produce_ttc(ttc)

    def __check_cant_produce(self):
        H = self.U.get_self()

        for pt in [r"Power\s+Plant", r"^Engineer$", r"Lunar\s+Infantry"]:
            ttc = next(filter_ttc(self.type_classes, pt))
            equals(
                self.can_produce_ttc(ttc, H),
                False,
                f"can produce {ttc}",
            )

    def __sanity_checks(self):
        self.__check_cant_produce()

    async def game_step(self, s: ra2yr.GameState):
        """Called just _before_ setting self.state

        Parameters
        ----------
        s : ra2yr.GameState
            _description_
        """
        # if self.state.current_frame < self.profile_duration and not self.pr_on:
        #     self.pr.enable()
        #     self.pr_on = True
        # if self.state.current_frame >= self.profile_duration and self.pr_on:
        # lg.debug("%s", finish_profiler(self.pr))
        #     self.pr_on = False

        if not self.type_classes:
            lg.debug("no type classes yet")
            return

        self.U.set_state(s)
        self.setup_ttc_map()
        # self.__sanity_checks()

        await self.clear_completed_tasks()
        if not self.conyards():
            mcv = self.mcvs()[0]
            if not mcv.deploying or mcv.deployed:
                return self.deploy_main_conyard
        await self.place_buildings()
        next_building = await self.choose_building_to_produce()
        if next_building:
            await self.try_produce_ttc(next_building)
        await self.explore_shrouded()
        if len(self.scout_units()) >= self._num_scout_units:
            await self.capture_buildings()
            await self.produce_attackers()

    async def step(self, s: ra2yr.GameState):
        if self.started:
            return await self.game_step(s)


async def complex_test():
    M = MyManager(poll_frequency=20)
    M.start()
    lg.info("wait game to start")
    await M.M.wait_game_to_begin()
    lg.info("wait game to exit")
    await M.M.wait_game_to_exit(timeout=None)


async def main():
    t = logged_task(complex_test())
    await t


if __name__ == "__main__":
    setup_logging(level=lg.DEBUG)
    asyncio.run(main())
