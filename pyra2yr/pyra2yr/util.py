import datetime
import io
import logging
import math
import re
from enum import Enum
from typing import Iterable, Iterator, List, Tuple

import google.protobuf.text_format as text_format
from google.protobuf.internal.decoder import _DecodeVarint32
from ra2yrproto import ra2yr


def tuple2coord(x) -> ra2yr.Coordinates:
    return ra2yr.Coordinates(**{k: x[i] for i, k in enumerate("xyz")})


def unixpath(p: str):
    return re.sub(r"^\w:", "", re.sub(r"\\+", "/", p))


def coord2cell(x) -> Tuple[int, int, int]:
    if isinstance(x, tuple):
        return tuple(coord2cell(v) for v in x)
    return int(x / 256)


def cell2coord(x) -> Tuple[int]:
    if isinstance(x, tuple):
        return tuple(cell2coord(v) for v in x)
    return int(x * 256)


def coord2tuple(x: ra2yr.Coordinates) -> Tuple[int]:
    return tuple(getattr(x, k) for k in "xyz")


def pdist(v1, v2, n=2):
    return math.sqrt(
        sum((x0 - x1) ** n for i, (x0, x1) in enumerate(zip(v1, v2)))
    )


def ind2sub(i, x0, x1):
    r = int(i / x0)
    return (i - r * x0, r)


def sub2ind(i0, i1, b0, b1):
    return i0 * b0 + i1


class StateUtil:
    def __init__(self, type_classes: List[ra2yr.TypeClass]):
        self.type_classes = type_classes
        self._state = None

    def set_state(self, state: ra2yr.GameState):
        self._state = state

    @property
    def state(self) -> ra2yr.GameState:
        if not self._state:
            raise RuntimeError("no state")
        return self._state

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


def msg_oneline(m):
    return text_format.MessageToString(m, as_one_line=True)


def equals(a, b, msg=""):
    if not msg:
        msg = f"{a} != {b}"

    assert a == b, msg


def set_equals(a, b, msg=""):
    sa = set(a)
    sb = set(b)

    equals(sa, sb, msg)


class EventListID(Enum):
    out_list = 1
    do_list = 2
    megamission_list = 3


class EventHistory:
    def __init__(self, lists: ra2yr.EventListsSnapshot):
        self.lists = lists

    def unique_events(self):
        """Get unique events across all of the event queues for the entire timespan"""
        all_events = []
        for l in self.lists:
            for k in ["out_list", "do_list", "megamission_list"]:
                all_events.extend(
                    (k, x.frame, x.timing, x) for x in getattr(l, k)
                )
        keys = [(x[0], x[1], x[2]) for x in all_events]
        res = []
        for u in set(keys):
            res.append(all_events[keys.index(u)])
        return res

    def get_indexed(self) -> List[List[Tuple[EventListID, ra2yr.Event]]]:
        """For each frame, flattens events from each event list into a single list.

        This is useful for determining the point when an event was consumed.
        """
        res = []
        for l in self.lists.lists:
            flattened = []
            for k in EventListID:
                events = getattr(l, k.name)
                flattened.extend((k, x) for x in events)
            res.append(flattened)
        return res


def find_objects(s: ra2yr.GameState, o: ra2yr.Object):
    return (x for x in s.objects if o.pointer_self == x.pointer_self)


def setup_logging(level=logging.INFO):
    FORMAT = "[%(levelname)s] %(asctime)s %(module)s.%(filename)s:%(lineno)d: %(message)s"
    logging.basicConfig(level=level, format=FORMAT)
    start_time = datetime.datetime.now().isoformat()
    level_name = logging.getLevelName(level)
    logging.info("Logging started at: %s, level=%s", start_time, level_name)


class Clock:
    def __init__(self):
        self.t = self.tic()

    def tic(self):
        self.t = datetime.datetime.now()
        return self.t

    def toc(self):
        return (datetime.datetime.now() - self.t).total_seconds()


def cell_to_coordinates(i: int, bounds: Tuple[int, int]):
    """Convert index to Coordinates (leptons) format

    Parameters
    ----------
    i : int
        _description_
    bounds : Tuple[int, int]
        _description_
    """
    x0, x1 = ind2sub(i, *bounds)
    return cell2coord((x0 + 0.5, x1 + 0.5, 0))


def finish_profiler(p):
    p.disable()
    s = io.StringIO()
    ps = pstats.Stats(p, stream=s).sort_stats(SortKey.CUMULATIVE)
    ps.print_stats()
    return s.getvalue()
