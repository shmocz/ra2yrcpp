import re
import datetime
import math
import gzip
from typing import List, Iterable, Iterator
from ra2yrproto import ra2yr
from google.protobuf.internal.decoder import _DecodeVarint32


def tuple2coord(x):
    return ra2yr.Coordinates(**{k: x[i] for i, k in enumerate("xyz")})


def unixpath(p: str):
    return re.sub(r"^\w:", "", re.sub(r"\\+", "/", p))


def coord2cell(x):
    if isinstance(x, tuple):
        return tuple(coord2cell(v) for v in x)
    return int(x / 256)


def coord2tuple(x):
    return tuple(getattr(x, k) for k in "xyz")


def pdist(v1, v2, n=2):
    return math.sqrt(
        sum((x0 - x1) ** n for i, (x0, x1) in enumerate(zip(v1, v2)))
    )


class StateUtil:
    def __init__(self, type_classes: List[ra2yr.TypeClass]):
        self.type_classes = type_classes
        self.state = None

    def set_state(self, state: ra2yr.GameState):
        self.state = state

    def get_house(self, name: str) -> ra2yr.House:
        return next(h for h in self.state.houses if h.name == name)

    def get_units(self, house_name: str) -> Iterable[ra2yr.Object]:
        h = self.get_house(house_name)
        return (u for u in self.state.objects if u.pointer_house == h.self)

    def get_production(self, house_name: str) -> ra2yr.Factory:
        h = self.get_house(house_name)
        return (f for f in self.state.factories if f.owner == h.self)

    def get_self(self) -> ra2yr.House:
        return next(h for h in self.state.houses if h.current_player)


class Clock:
    def __init__(self):
        self.t = None

    def tick(self):
        self.t = datetime.datetime.now()

    def tock(self):
        return (datetime.datetime.now() - self.t).total_seconds()


def read_protobuf_messages(f) -> Iterator[ra2yr.GameState]:
    buf = f.read(10)  # Maximum length of length prefix
    while buf:
        msg_len, new_pos = _DecodeVarint32(buf, 0)
        buf = buf[new_pos:]
        buf += f.read(msg_len - len(buf))
        s = ra2yr.GameState()
        s.ParseFromString(buf)
        yield s
        buf = buf[msg_len:]
        buf += f.read(10 - len(buf))
