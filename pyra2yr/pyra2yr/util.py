import datetime
import io
import logging
import pstats
import re
from enum import Enum, IntFlag, auto
from typing import Iterator, List, Tuple

import google.protobuf.text_format as text_format
import numpy as np
from google.protobuf.internal.decoder import _DecodeVarint32
from ra2yrproto import ra2yr


def tuple2coord(x) -> ra2yr.Coordinates:
    return ra2yr.Coordinates(**{k: x[i] for i, k in enumerate("xyz")})


def unixpath(p: str):
    return re.sub(r"^\w:", "", re.sub(r"\\+", "/", p))


def coord2cell(x):
    if isinstance(x, tuple):
        return tuple(coord2cell(v) for v in x)
    return int(x / 256)


def cell2coord(x) -> Tuple[int]:
    if isinstance(x, tuple):
        return tuple(cell2coord(v) for v in x)
    return int(x * 256)


def coord2tuple(x: ra2yr.Coordinates) -> Tuple[int]:
    return tuple(getattr(x, k) for k in "xyz")


def array2coord(x: np.array) -> ra2yr.Coordinates:
    return ra2yr.Coordinates(**{k: int(v) for k, v in zip("xyz", x)})


def coord2array(x: ra2yr.Coordinates) -> np.array:
    return np.array(coord2tuple(x))


def pdist(x1, x2, axis=0):
    return np.sqrt(np.sum((x1 - x2) ** 2, axis=axis))


def ind2sub(i, x0, x1):
    r = int(i / x0)
    return (i - r * x0, r)


def sub2ind(i0, i1, b0, b1):
    return i0 * b0 + i1


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


def finish_profiler(p):
    p.disable()
    s = io.StringIO()
    ps = pstats.Stats(p, stream=s).sort_stats(pstats.SortKey.CUMULATIVE)
    ps.print_stats()
    return s.getvalue()


class PrerequisiteCheck(IntFlag):
    OK = auto()
    EMPTY_PREREQUISITES = auto()
    NOT_IN_MYIDS = auto()
    INVALID_PREQ_GROUP = auto()
    NOT_PREQ_GROUP_IN_MYIDS = auto()
    BAD_TECH_LEVEL = auto()
    NO_STOLEN_TECH = auto()
    FORBIDDEN_HOUSE = auto()
    BUILD_LIMIT_REACHED = auto()


def check_stolen_tech(ttc: ra2yr.ObjectTypeClass, h: ra2yr.House):
    return (
        (ttc.requires_stolen_allied_tech and not h.allied_infiltrated)
        or (ttc.requires_stolen_soviet_tech and not h.soviet_infiltrated)
        or (ttc.requires_stolen_third_tech and not h.third_infiltrated)
    )


def check_preq_list(ttc: ra2yr.ObjectTypeClass, myids, prerequisite_map):
    res = PrerequisiteCheck.OK
    if not ttc.prerequisites:
        res |= PrerequisiteCheck.EMPTY_PREREQUISITES
    for p in ttc.prerequisites:
        if p >= 0 and p not in myids:
            res |= PrerequisiteCheck.NOT_IN_MYIDS
        if p < 0:
            if p not in prerequisite_map:
                res |= PrerequisiteCheck.INVALID_PREQ_GROUP
            if not myids.intersection(prerequisite_map[p]):
                res |= PrerequisiteCheck.NOT_PREQ_GROUP_IN_MYIDS
    return res


def cell_grid(coords, rx: int, ry: int) -> List[ra2yr.Coordinates]:
    """Get a rectangular grid centered on given coordinates.

    Parameters
    ----------
    coords : _type_
       The center coordinates for the grid.
    rx : int
        x-axis radius
    ry : int
        y-axis radius

    Returns
    -------
    List[ra2yr.Coordinates]
        List of cell coordinates lying on the given bounds.
    """
    # TODO(shmocz): generalize
    if coords.size < 3:
        coords = np.append(coords, 0)
    place_query_grid = []
    for i in range(0, rx):
        for j in range(0, ry):
            place_query_grid.append(
                tuple2coord(
                    tuple(
                        x + y * 256
                        for x, y in zip(
                            coords, (i - int(rx / 2), j - int(ry / 2), 0)
                        )
                    )
                )
            )
    return place_query_grid
