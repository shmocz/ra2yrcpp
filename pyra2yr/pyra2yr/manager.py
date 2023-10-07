import asyncio
import logging as lg
import traceback
from collections.abc import Iterable
from datetime import datetime as dt
from enum import Enum
from typing import Any

import numpy as np
from ra2yrproto import commands_builtin, commands_game, commands_yr, core, ra2yr

from pyra2yr.network import DualClient, logged_task
from pyra2yr.state_manager import StateManager
from pyra2yr.util import Clock, cell_grid


class PlaceStrategy(Enum):
    RANDOM = 0
    FARTHEST = 1
    ABSOLUTE = 2


class CachedEntry:
    def __init__(self, s: ra2yr.GameState):
        self.s = s
        self.last_updated = 0
        self.value = None

    def should_update(self) -> bool:
        return self.last_updated < self.s.current_frame

    def get(self) -> Any:
        return self.value

    def update(self, x: Any):
        self.value = x
        self.last_updated = self.s.current_frame


class NumpyMapData:
    def __init__(self, data: ra2yr.MapDataSoA):
        self.data = data
        self.width = data.map_width
        self.height = data.map_height
        self.keys = [
            "land_type",
            "height",
            "level",
            "overlay_data",
            "tiberium_value",
            "shrouded",
            "passability",
        ]

        self.dm0 = np.zeros(
            (self.width * self.height, len(self.keys)), dtype=np.int32
        )

        self.update(data)

    def update(self, data: ra2yr.MapDataSoA):
        self.dm0[:, 0] = data.land_type[:]
        self.dm0[:, 5] = data.shrouded[:]
        self.dm0[:, 6] = data.passability[:]
        self.dm = self.dm0.reshape((self.width, self.height, len(self.keys)))
        self.data = data


class Manager:
    """Manages connections and state updates for an active game process."""

    def __init__(
        self,
        address: str = "0.0.0.0",
        port: int = 14521,
        poll_frequency=20,
        fetch_state_timeout=5.0,
    ):
        """
        Parameters
        ----------
        address : str, optional
            WebSocket API endpoint, by default "0.0.0.0"
        port : int, optional
            Destination server port, by default 14525
        poll_frequency : int, optional
            Frequency for polling the game state in Hz, by default 20
        fetch_state_timeout : float, optional
            Timeout (seconds) for state fetching (default: 5.0)
        """
        self.address = address
        self.port = port
        self.poll_frequency = min(max(1, poll_frequency), 60)
        self.fetch_state_timeout = fetch_state_timeout
        # self.state: ra2yr.GameState = ra2yr.GameState()
        self.state = StateManager()
        self.type_classes: list[ra2yr.ObjectTypeClass] = []
        self.prerequisite_groups: ra2yr.PrerequisiteGroups = []
        self.callbacks = []
        self._stop = asyncio.Event()
        self.client: DualClient = DualClient(self.address, self.port)
        self.t = Clock()
        self.iters = 0
        self.show_stats_every = 30
        self.delta = 0
        self.M = ManagerUtil(self)

    def start(self):
        self._main_task = logged_task(self.mainloop())
        self.client.connect()

    async def stop(self):
        self._stop.set()
        await self._main_task
        await self.client.stop()

    async def step(self, s: ra2yr.GameState):
        pass

    async def update_initials(self):
        res_istate = await self.M.read_value(
            initial_game_state=ra2yr.GameState()
        )
        state = res_istate.data.initial_game_state
        self.state.set_initials(state.object_types, state.prerequisite_groups)

    async def on_state_update(self, s: ra2yr.GameState):
        if self.iters % self.show_stats_every == 0:
            delta = self.t.toc()
            lg.debug(
                "step=%d interval=%d avg_duration=%f avg_fps=%f",
                self.iters,
                self.show_stats_every,
                delta / self.show_stats_every,
                self.show_stats_every / delta,
            )
            self.t.tic()
        if s.current_frame > 0:
            if not self.state.has_initials():
                await self.update_initials()
            try:
                fn = await self.step(s)
                if fn:
                    # await asyncio.create_task(fn)
                    await fn()
            except AssertionError:
                raise
            except:
                lg.error("exception on step: %s", traceback.format_exc())
        self.iters += 1

    async def get_state(self) -> ra2yr.GameState:
        cmd = commands_yr.GetGameState()
        state = await self.client.exec_command(cmd, timeout=5.0)
        if not state.result.Unpack(cmd):
            raise RuntimeError(f"failed to unpack state: {state}")
        return cmd.state

    async def run_command(self, c: Any) -> core.CommandResult:
        return await self.client.exec_command(c)

    async def run(self, c: Any = None, **kwargs) -> Any:
        for k, v in kwargs.items():
            if isinstance(v, list):
                getattr(c, k).extend(v)
            else:
                try:
                    setattr(c, k, v)
                except:  # FIXME: more explicit check
                    getattr(c, k).CopyFrom(v)
        cmd_name = c.__class__.__name__
        res = await self.run_command(c)
        if res.result_code == core.ResponseCode.ERROR:
            lg.error(
                "Failed to run command %s: %s", cmd_name, res.error_message
            )
        res_o = type(c)()
        res.result.Unpack(res_o)
        return res_o

    # TODO: dont run async code in same thread as Manager due to performance reasons
    async def mainloop(self):
        d = 1 / self.poll_frequency
        deadline = dt.now().timestamp()
        while not self._stop.is_set():
            try:
                await asyncio.sleep(
                    min(d, max(deadline - dt.now().timestamp(), 0.0))
                )
                deadline = dt.now().timestamp() + d
                s = await self.get_state()
                if not self.state.should_update(s):
                    continue
                self.state.set_state(s)
                await self.on_state_update(s)
                await self.state.state_updated()
            except asyncio.exceptions.TimeoutError:
                lg.error("Couldn't fetch result")

    # FIXME(shmocz): deprecate
    async def wait_state(self, cond, timeout=30, err=None):
        await self.state.wait_state(lambda x: cond(), timeout=timeout, err=err)

    async def wait_state2(self, cond, timeout=30, err=None):
        await self.state.wait_state(cond, timeout=timeout, err=err)

    def players(self) -> list[ra2yr.House]:
        return [
            p
            for p in self.state.s.houses
            if p.name not in ["Special", "Neutral"]
        ]


class CommandBuilder:
    @classmethod
    def make_command(cls, c: Any, **kwargs):
        for k, v in kwargs.items():
            if v is None:
                continue
            if isinstance(v, list):
                getattr(c, k).extend(v)
            else:
                try:
                    setattr(c, k, v)
                except:  # FIXME: more explicit check
                    getattr(c, k).CopyFrom(v)
        return c

    @classmethod
    def add_event(
        cls,
        event_type: ra2yr.NetworkEvent = None,
        house_index: int = 0,
        frame_delay=0,
        spoof=False,
        **kwargs,
    ):
        return cls.make_command(
            commands_yr.AddEvent(),
            event=ra2yr.Event(
                event_type=event_type, house_index=house_index, **kwargs
            ),
            frame_delay=frame_delay,
            spoof=spoof,
        )

    @classmethod
    def make_place(
        cls,
        heap_id=None,
        is_naval=None,
        location=None,
    ) -> commands_yr.AddEvent:
        return cls.add_event(
            event_type=ra2yr.NETWORK_EVENT_Place,
            place=ra2yr.Event.Place(
                rtti_type=ra2yr.ABSTRACT_TYPE_BUILDINGTYPE,
                heap_id=heap_id,
                is_naval=is_naval,
                location=location,
            ),
        )

    @classmethod
    def make_produce(
        cls, rtti_id: int = 0, heap_id: int = 0, is_naval: bool = False
    ):
        return cls.add_event(
            event_type=ra2yr.NETWORK_EVENT_Produce,
            production=ra2yr.Event.Production(
                rtti_id=rtti_id, heap_id=heap_id, is_naval=is_naval
            ),
        )

    @classmethod
    def make_produce_order(
        cls, object_type=ra2yr.ObjectTypeClass, action=ra2yr.ProduceAction
    ):
        return cls.make_command(
            commands_game.ProduceOrder(),
            object_type=object_type,
            action=action,
        )

    @classmethod
    def mission_clicked(
        cls,
        object_addresses: list[int],
        event: ra2yr.Mission,
        coordinates=None,
        target_object=None,
    ):
        return cls.make_command(
            commands_yr.MissionClicked(),
            object_addresses=object_addresses,
            event=event,
            coordinates=coordinates,
            target_object=target_object,
        )

    @classmethod
    def click_event(
        cls,
        event_type: ra2yr.NetworkEvent = None,
        object_addresses: list[int] = None,
        **kwargs,
    ):
        return cls.make_command(
            commands_yr.ClickEvent(),
            object_addresses=object_addresses,
            event=event_type,
        )

    @classmethod
    def unit_command(
        cls,
        object_addresses: list[int] = None,
        action: ra2yr.UnitAction = None,
    ):
        return cls.make_command(
            commands_yr.UnitCommand(),
            object_addresses=object_addresses,
            action=action,
        )

    @classmethod
    def unit_order(
        cls,
        object_addresses: list[int] = None,
        action: ra2yr.UnitAction = None,
        target_object: int = None,
        coordinates=None,
    ):
        return cls.make_command(
            commands_game.UnitOrder(),
            object_addresses=object_addresses,
            action=action,
            target_object=target_object,
            coordinates=coordinates,
        )


class ManagerUtil:
    def __init__(self, manager: Manager):
        self.manager = manager
        self.C = CommandBuilder

    # TODO(shmocz): low level stuff put elsewhere
    async def unit_command(
        self,
        object_addresses: list[int] = None,
        action: ra2yr.UnitAction = None,
    ):
        return await self.manager.run(
            self.C.unit_command(
                object_addresses=object_addresses, action=action
            )
        )

    async def select(
        self,
        object_addresses: list[int] = None,
    ):
        return await self.execute(
            commands_game.UnitOrder,
            object_addresses=object_addresses,
            action=ra2yr.UNIT_ACTION_SELECT,
        )

    async def move(self, object_addresses: list[int] = None, coordinates=None):
        return await self.execute(
            commands_game.UnitOrder,
            object_addresses=object_addresses,
            action=ra2yr.UNIT_ACTION_MOVE,
            coordinates=coordinates,
        )

    async def capture(
        self,
        object_addresses: list[int] = None,
        coordinates=None,
        target_object: int = 0,
    ):
        return await self.manager.run(
            self.C.mission_clicked(
                object_addresses=object_addresses,
                event=ra2yr.Mission_Capture,
                coordinates=coordinates,
                target_object=target_object,
            )
        )

    # TODO(shmocz): wait for proper value
    async def deploy(self, object_address: int = None):
        return await self.manager.run(
            self.C.unit_order(
                object_addresses=[object_address],
                action=ra2yr.UNIT_ACTION_DEPLOY,
            )
        )

    async def execute(self, command_class: Any, **kwargs):
        return await self.manager.run(
            self.C.make_command(command_class(), **kwargs)
        )

    async def place_query(
        self, type_class: int = None, house_class: int = None, coordinates=None
    ) -> commands_yr.PlaceQuery:
        return await self.manager.run(
            self.C.make_command(
                commands_yr.PlaceQuery(),
                type_class=type_class,
                house_class=house_class,
                coordinates=coordinates,
            )
        )

    async def place_building(
        self,
        building: ra2yr.Object = None,
        coordinates: ra2yr.Coordinates = None,
    ) -> core.CommandResult:
        return await self.run_command(
            commands_game.PlaceBuilding(
                building=building, coordinates=coordinates
            ),
        )

    async def click_event(
        self, object_addresses=None, event: ra2yr.NetworkEvent = None
    ):
        return await self.manager.run(
            self.C.make_command(
                commands_yr.ClickEvent(),
                object_addresses=object_addresses,
                event=event,
            )
        )

    async def sell_buildings(self, object_addresses=None):
        return await self.execute(
            commands_game.UnitOrder,
            object_addresses=object_addresses,
            action=ra2yr.UNIT_ACTION_SELL,
        )

    async def sell_building(self, object_address=None):
        return await self.sell_buildings(object_addresses=[object_address])

    async def produce_order(
        self,
        object_type: ra2yr.ObjectTypeClass,
        action: ra2yr.ProduceAction = None,
    ) -> core.CommandResult:
        return await self.run_command(
            commands_game.ProduceOrder(object_type=object_type, action=action)
        )

    async def unit_order(
        self,
        objects: list[ra2yr.Object] = None,
        action: ra2yr.UnitAction = None,
        target_object: ra2yr.Object = None,
        coordinates: ra2yr.Coordinates = None,
    ):
        p_target = None
        if target_object:
            p_target = target_object.pointer_self
        if not isinstance(objects, list):
            if isinstance(objects, Iterable):
                objects = list(objects)
            elif not objects is None:
                objects = [objects]
            else:
                objects = []
        return await self.manager.run_command(
            commands_game.UnitOrder(
                object_addresses=[o.pointer_self for o in objects],
                action=action,
                target_object=p_target,
                coordinates=coordinates,
            )
        )

    async def sell(self, objects: list[ra2yr.Object]):
        return await self.unit_order(
            objects=objects, action=ra2yr.UNIT_ACTION_SELL
        )

    async def sell_walls(self, coordinates: ra2yr.Coordinates):
        return await self.unit_order(
            action=ra2yr.UNIT_ACTION_SELL_CELL, coordinates=coordinates
        )

    async def produce(
        self,
        rtti_id: int = 0,
        heap_id: int = 0,
        is_naval: bool = False,
    ):
        return await self.manager.run(
            self.C.make_produce(
                rtti_id=rtti_id,
                heap_id=heap_id,
                is_naval=is_naval,
            )
        )

    # TODO(shmocz): autodetect is_naval in the library
    async def produce_building(self, heap_id: int = 0, is_naval: bool = False):
        return await self.manager.run(
            self.C.make_produce(
                rtti_id=ra2yr.ABSTRACT_TYPE_BUILDINGTYPE,
                heap_id=heap_id,
                is_naval=is_naval,
            )
        )

    async def run_command(self, c: Any):
        return await self.manager.run_command(c)

    async def start_production(
        self, object_type: ra2yr.ObjectTypeClass
    ) -> core.CommandResult:
        return await self.produce_order(
            object_type=object_type,
            action=ra2yr.PRODUCE_ACTION_BEGIN,
        )

    async def add_message(
        self,
        message: str = None,
        duration_frames: int = None,
        color: ra2yr.ColorScheme = None,
    ):
        return await self.manager.run(
            self.C.make_command(
                commands_yr.AddMessage(),
                message=message,
                duration_frames=duration_frames,
                color=color,
            )
        )

    async def read_value(self, **kwargs):
        return await self.manager.run(
            commands_yr.ReadValue(),
            data=ra2yr.StorageValue(**kwargs),
        )

    async def inspect_configuration(
        self, config: commands_yr.Configuration = None
    ):
        return await self.manager.run(
            self.C.make_command(
                commands_yr.InspectConfiguration(), config=config
            )
        )

    async def get_system_state(self):
        return await self.manager.run(commands_builtin.GetSystemState())

    async def get_place_locations(
        self, coords, type_class_id: int, house_pointer: int, rx: int, ry: int
    ):
        res = await self.place_query(
            type_class=type_class_id,
            house_class=house_pointer,
            coordinates=cell_grid(coords, rx, ry),
        )
        return res

    async def wait_game_to_begin(self, timeout=60):
        await self.manager.wait_state2(
            lambda x: x.s.stage == ra2yr.STAGE_INGAME and x.s.current_frame > 1,
            timeout=timeout,
        )

    async def wait_game_to_exit(self, timeout=60):
        await self.manager.wait_state(
            lambda: self.manager.state.s.stage == ra2yr.STAGE_EXIT_GAME,
            timeout=timeout,
        )


class ActionTracker:
    def __init__(self, s: ra2yr.GameState):
        self.s = s
        self.rules = {}
        self.values = {}

    def add_tracker(self, key, frames):
        self.rules[key] = frames
        self.values[key] = 0

    def proceed(self, key):
        if self.s.current_frame - self.values[key] > self.rules[key]:
            self.values[key] = self.s.current_frame
            return True
        return False
