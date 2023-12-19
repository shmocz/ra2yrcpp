import numpy as np
from ra2yrproto import ra2yr

from pyra2yr.state_container import StateContainer
from pyra2yr.util import coord2array


class ViewObject:
    """Provides an up to date view to an object of the underlying game state.
    State change is directly reflected in the view object.
    """

    def __init__(self, m: StateContainer):
        self.m = m
        self._invalid = False
        self.latest_frame = -1

    def invalid(self):
        self.update()
        return self._invalid

    def fetch_next(self):
        raise NotImplementedError()

    def update(self):
        if self._invalid:
            return
        if self.latest_frame != self.m.s.current_frame:
            try:
                self.fetch_next()
                self.latest_frame = self.m.s.current_frame
            # TODO: general exception
            except StopIteration:
                self._invalid = True


class ObjectEntry(ViewObject):
    def __init__(self, m: StateContainer, o: ra2yr.Object):
        super().__init__(m)
        self.o: ra2yr.Object = o

    def fetch_next(self):
        self.o = self.m.get_object(self.o)

    def get(self) -> ra2yr.Object:
        """Get reference to most recent Object entry.

        Returns
        -------
        ra2yr.Object
        """
        self.update()
        return self.o

    def tc(self) -> ra2yr.ObjectTypeClass:
        """The type class of the object"""
        return self.m.ttc_map[self.o.pointer_technotypeclass]

    @property
    def coordinates(self):
        return coord2array(self.get().coordinates)

    @property
    def health(self) -> float:
        """Health in percentage

        Returns
        -------
        float
            Health in percentage
        """
        return self.get().health / self.tc().strength

    def __repr__(self):
        return str(self.o)


class FactoryEntry(ViewObject):
    """State object entry.

    If the object is no longer in state, it's marked as invalid.
    """

    def __init__(self, m: StateContainer, o: ra2yr.Factory):
        super().__init__(m)
        self.o: ra2yr.Factory = o
        self.object = ObjectEntry(m, m.get_object(o.object))

    def fetch_next(self):
        self.o = self.m.get_factory(self.o)

    def get(self) -> ra2yr.Factory:
        """Get reference to most recent Factory entry.

        Returns
        -------
        ra2yr.Factory
        """
        self.update()
        return self.o


class MapData:
    def __init__(self, m: ra2yr.MapData):
        self.m = m

    def ind2sub(self, I):
        yy, xx = np.unravel_index(I, (self.m.width, self.m.height))
        return np.c_[xx, yy]

    @classmethod
    def bbox(cls, x):
        m_min = np.min(x, axis=0)
        m_max = np.max(x, axis=0)
        return np.array(
            [
                [m_max[0] + 1, m_max[1] + 1],  # BL
                [m_max[0] + 1, m_min[1] - 1],  # BR
                [m_min[0] - 1, m_max[1] + 1],  # TL
                [m_min[0] - 1, m_min[1] - 1],  # TR
            ]
        )
