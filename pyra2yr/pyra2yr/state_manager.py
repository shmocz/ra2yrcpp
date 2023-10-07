import asyncio
import json
import logging as lg
import re
from functools import cached_property
from typing import Iterator, Type

from google.protobuf import message as _message
from google.protobuf.json_format import MessageToDict
from ra2yrproto import ra2yr

from pyra2yr.util import msg_oneline


class ObjectMissing(Exception):
    pass


class StateManager:
    def __init__(self, s: ra2yr.GameState = None):
        if not s:
            s = ra2yr.GameState()
        self.s = s
        self._types = None
        self._prerequisite_groups: ra2yr.PrerequisiteGroups = []
        self._cond_state_update = asyncio.Condition()

    def set_state(self, s: ra2yr.GameState):
        self.s.CopyFrom(s)

    def should_update(self, s: ra2yr.GameState) -> bool:
        return (
            s.current_frame != self.s.current_frame or s.stage != self.s.stage
        )

    def set_initials(
        self, t: list[ra2yr.ObjectTypeClass], p: ra2yr.PrerequisiteGroups
    ):
        self._types = t
        self._prerequisite_groups = p

    def types(self) -> list[ra2yr.ObjectTypeClass]:
        return self._types

    @cached_property
    def prerequisite_map(self) -> dict[int, set[int]]:
        items = {
            "proc": -6,
            "tech": -5,
            "radar": -4,
            "barracks": -3,
            "factory": -2,
            "power": -1,
        }
        return {
            v: set(getattr(self._prerequisite_groups, k))
            for k, v in items.items()
        }

    @cached_property
    def ttc_map(self) -> dict[int, ra2yr.ObjectTypeClass]:
        return {x.pointer_self: x for x in self._types}

    def has_initials(self) -> bool:
        return self._prerequisite_groups and self._types

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

    def get_objects(
        self, t: ra2yr.ObjectTypeClass | int | None = None
    ) -> Iterator[ra2yr.Object]:
        if not t:
            return (o for o in self.s.objects)
        if isinstance(t, ra2yr.ObjectTypeClass):
            t = t.pointer_self
        return (x for x in self.s.objects if x.pointer_technotypeclass == t)

    def get_object(self, o: ra2yr.Object | int) -> ra2yr.Object:
        """Get object from current state by pointer value.

        Parameters
        ----------
        o : ra2yr.Object | int | None
            Object or address to be queried. If None, return all objects

        Returns
        -------
        ra2yr.Object
            _description_

        Raises
        ------
        StopIteration if object wasn't found
        """
        if isinstance(o, ra2yr.Object):
            o = o.pointer_self
        try:
            return next(x for x in self.s.objects if x.pointer_self == o)
        except StopIteration:
            lg.error("fatal: object %s not found from %s", o, self.s.objects)
            with open("state.dump.json", "w") as f:
                f.write(json.dumps(MessageToDict(self.s)))
            raise

    def get_type_class_by_regex(
        self, p: str
    ) -> Iterator[ra2yr.ObjectTypeClass]:
        return (t for t in self.types() if re.search(p, t.name))

    def query_type_class(self, p: str, abstract_type=None):
        x = (t for t in self.get_type_class_by_regex(p))
        if abstract_type is not None:
            return (s for s in x if s.type == abstract_type)
        return x


class StateObject:
    def __init__(self, m: StateManager):
        self.m = m

    def update(self):
        raise NotImplementedError()

    def fetch_next(self):
        raise NotImplementedError()

    def to_string(self):
        raise NotImplementedError()


class ProtobufStateObject(StateObject):
    def __init__(self, m: StateManager):
        super().__init__(m)
        self.o: Type[_message] = None

    def update(self):
        try:
            x = self.fetch_next()
            self.o = x
        except Exception as e:
            raise ObjectMissing(self.to_string()) from e
        return self

    def to_string(self):
        return msg_oneline(self.o)


class Object(ProtobufStateObject):
    def __init__(self, m: StateManager, o: ra2yr.Object = None):
        super().__init__(m)
        self.o = o

    def tc(self) -> ra2yr.ObjectTypeClass:
        return self.m.ttc_map[self.o.pointer_technotypeclass]

    def fetch_next(self):
        return self.m.get_object(self.o)


class House(StateObject):
    def __init__(self, m: StateManager, o: ra2yr.House = None):
        super().__init__(m)
        self.o = o

    def fetch_next(self):
        return next(o for o in self.m.s.houses if o.self == self.o.self)


class ObjectFactory:
    def __init__(self, m: StateManager):
        self.m = m

    def create(self, o: ra2yr.Object) -> Object:
        return Object(o, self.m)

    def create_object(self, o: ra2yr.Object) -> Object:
        return Object(self.m, o)

    def create_house(self, o: ra2yr.House) -> House:
        return House(self.m, o)

    def create_object_type(self):
        raise NotImplementedError()
