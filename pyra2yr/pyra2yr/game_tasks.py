import logging as lg
from enum import Enum
from typing import List

from ra2yrproto import ra2yr

from pyra2yr.util import find_objects


class ProduceCategory(Enum):
    INVALID = -1
    BUILDING = 0
    DEFENSE = 1
    INFANTRY = 2
    TANK = 3
    NAVAL = 4
    AIRCRAFT = 5


class AsyncGameTask:
    """_summary_"""

    def __init__(self, s: ra2yr.GameState):
        self.s = s

    async def is_completed(self) -> bool:
        raise NotImplementedError()

    def equals(self, o) -> bool:
        raise NotImplementedError()


class PlaceTask(AsyncGameTask):
    def __init__(self, s: ra2yr.GameState, o: ra2yr.Object, deadline=60):
        super().__init__(s)
        self.o = o
        self.frame = s.current_frame
        self.deadline = deadline

    async def is_completed(self) -> bool:
        if self.s.current_frame - self.frame > self.deadline:
            lg.error("place task expired")
            return True
        facs = [f for f in self.s.factories if f.object == self.o.pointer_self]
        if facs:
            return False
        return True

    def equals(self, o) -> bool:
        if not isinstance(o, type(self)):
            return False
        return o.o.pointer_self == self.o.pointer_self


class ProduceTask(AsyncGameTask):
    def __init__(
        self,
        s: ra2yr.GameState,
        t: ra2yr.ObjectTypeClass,
        h: ra2yr.House,
        type_classes: List[ra2yr.ObjectTypeClass],
    ):
        super().__init__(s)
        self.t = t
        self.h = h
        self.type_classes = type_classes

    @staticmethod
    def get_produce_category(t: ra2yr.ObjectTypeClass):
        C = ProduceCategory
        if t.type == ra2yr.ABSTRACT_TYPE_BUILDINGTYPE:
            if t.build_category == ra2yr.BUILD_CATEGORY_Combat:
                return C.DEFENSE
            return C.BUILDING
        if t.naval:
            return C.NAVAL
        if t.type == ra2yr.ABSTRACT_TYPE_INFANTRYTYPE:
            return C.INFANTRY
        if t.type == ra2yr.ABSTRACT_TYPE_UNITTYPE:
            return C.TANK
        if t.type == ra2yr.ABSTRACT_TYPE_AIRCRAFTTYPE:
            return C.AIRCRAFT
        raise RuntimeError("Invalid produce category")

    def my_production(self):
        return [f for f in self.s.factories if f.owner == self.h.self]

    def already_producing(self) -> bool:
        keys = set()
        objs = [f.object for f in self.my_production()]
        for o in objs:
            obj = next(x for x in self.s.objects if x.pointer_self == o)
            ttc = next(
                t
                for t in self.type_classes
                if t.pointer_self == obj.pointer_technotypeclass
            )
            keys.add(self.get_produce_category(ttc))
        return self.get_produce_category(self.t) in keys

    async def is_completed(self) -> bool:
        if not self.type_classes:
            return False
        # FIXME: check queued objects too
        if self.already_producing():
            return True
        # FIXME: rename house's self to pointer_self
        facs = [f for f in self.s.factories if f.owner == self.h.self]
        objs = [f.object for f in facs]
        for o in objs:
            obj = next(x for x in self.s.objects if x.pointer_self == o)
            ttc = next(
                t
                for t in self.type_classes
                if t.pointer_self == obj.pointer_technotypeclass
            )
            if (
                self.t.type == ttc.type
                and self.t.array_index == ttc.array_index
            ):
                return True

        return False

    def equals(self, o) -> bool:
        if not isinstance(o, type(self)):
            return False
        return (
            o.h.array_index == self.h.array_index
            and o.t.pointer_self == self.t.pointer_self
        )


class DeployTask(AsyncGameTask):
    def __init__(self, s: ra2yr.GameState, o: ra2yr.Object):
        super().__init__(s)
        self.o = o

    async def is_completed(self) -> bool:
        l = list(find_objects(self.s, self.o))
        if not l:
            return False
        return l[0].deploying or l[0].deployed

    def equals(self, o) -> bool:
        if not isinstance(o, type(self)):
            return False
        return o.o.pointer_self == self.o.pointer_self
