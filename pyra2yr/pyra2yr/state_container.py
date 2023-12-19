from functools import cached_property

from ra2yrproto import ra2yr


class StateContainer:
    def __init__(self, s: ra2yr.GameState = None):
        if not s:
            s = ra2yr.GameState()
        self.s = s
        self._types: list[ra2yr.ObjectTypeClass] = []
        self._prerequisite_groups: ra2yr.PrerequisiteGroups = []

    def get_factory(self, o: ra2yr.Factory) -> ra2yr.Factory:
        """Get factory that's producing a particular object.

        Parameters
        ----------
        o : ra2yr.Factory
            Factory to query with

        Returns
        -------
        ra2yr.Factory
            Corresponding factory in the state.

        Raises
        ------
        StopIteration if factory wasn't found.
        """
        return next(x for x in self.s.factories if x.object == o.object)

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
        return next(x for x in self.s.objects if x.pointer_self == o)

    def set_initials(
        self, t: list[ra2yr.ObjectTypeClass], p: ra2yr.PrerequisiteGroups
    ):
        self._types = t
        self._prerequisite_groups = p

    def has_initials(self) -> bool:
        return self._prerequisite_groups and self._types

    def set_state(self, s: ra2yr.GameState):
        self.s.CopyFrom(s)
        if any(o.pointer_technotypeclass == 0 for o in self.s.objects):
            raise RuntimeError(
                f"zero TC, frame={self.s.current_frame}, objs={self.s.objects}"
            )

    def types(self) -> list[ra2yr.ObjectTypeClass]:
        return self._types

    @cached_property
    def ttc_map(self) -> dict[int, ra2yr.ObjectTypeClass]:
        """Map pointer to type class for fast look ups.

        Returns
        -------
        dict[int, ra2yr.ObjectTypeClass]
            the mapping
        """
        return {x.pointer_self: x for x in self._types}

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
