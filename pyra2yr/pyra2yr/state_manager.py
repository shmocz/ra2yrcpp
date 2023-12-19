import asyncio
import logging as lg
import re
from typing import Iterator

from google.protobuf import message as _message
from ra2yrproto import ra2yr

from pyra2yr.state_container import StateContainer
from pyra2yr.state_objects import FactoryEntry, ObjectEntry


class StateManager:
    def __init__(self, s: ra2yr.GameState = None):
        self.sc = StateContainer(s)
        self._cond_state_update = asyncio.Condition()

    @property
    def s(self) -> ra2yr.GameState:
        return self.sc.s

    def should_update(self, s: ra2yr.GameState) -> bool:
        return (
            s.current_frame != self.s.current_frame or s.stage != self.s.stage
        )

    async def wait_state(self, cond, timeout=30, err=None):
        async with self._cond_state_update:
            try:
                await asyncio.wait_for(
                    self._cond_state_update.wait_for(lambda: cond(self)),
                    timeout,
                )
            except TimeoutError:
                if err:
                    lg.error("wait failed: %s", err)
                raise

    async def state_updated(self):
        async with self._cond_state_update:
            self._cond_state_update.notify_all()

    def query_type_class(
        self, p: str, abstract_type=None
    ) -> Iterator[ra2yr.ObjectTypeClass]:
        """Query type classes by pattern and abstract type

        Parameters
        ----------
        p : str
            Regex to be searched from type class name
        abstract_type : _type_, optional
            Abstract type of the type class

        Yields
        ------
        Iterator[ra2yr.ObjectTypeClass]
            Matching type classes
        """
        for x in self.sc.types():
            if (not re.search(p, x.name)) or (
                abstract_type and x.type != abstract_type
            ):
                continue
            yield x

    def query_objects(
        self,
        t: ra2yr.ObjectTypeClass = None,
        h: ra2yr.House = None,
        a: ra2yr.AbstractType = None,
        p: str = None,
    ) -> Iterator[ObjectEntry]:
        for _, x in enumerate(self.s.objects):
            o = ObjectEntry(self.sc, x)
            if (
                (h and o.get().pointer_house != h.self)
                or (t and t.pointer_self != o.get().pointer_technotypeclass)
                or (a and o.get().object_type != a)
                or (p and not re.search(p, o.tc().name))
            ):
                continue
            yield o

    def query_factories(
        self, t: ra2yr.ObjectTypeClass = None, h: ra2yr.House = None
    ) -> Iterator[FactoryEntry]:
        for x in self.s.factories:
            f = FactoryEntry(self.sc, x)
            if (h and f.o.owner != h.self) or (
                t and t.pointer_self != f.object.get().pointer_technotypeclass
            ):
                continue
            yield f

    def current_player(self) -> ra2yr.House:
        return next(p for p in self.s.houses if p.current_player)
