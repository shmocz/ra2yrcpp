import asyncio
import logging
from typing import Any, List
from ra2yrproto import commands_yr, core, ra2yr, commands_builtin
from pyra2yr.network import DualClient, log_exceptions
from pyra2yr.util import tuple2coord


error = logging.error
debug = logging.debug


class Manager:
    def __init__(
        self, address: str = "0.0.0.0", port: int = 14525, poll_frequency=20
    ):
        """_summary_

        Parameters
        ----------
        address : str, optional
            WebSocket API endpoint, by default "0.0.0.0"
        port : int, optional
            Destination server port, by default 14525
        poll_frequency : int, optional
            Frequency for polling the game state in Hz, by default 20
        """
        self.address = address
        self.port = port
        self.poll_frequency = min(max(1, poll_frequency), 60)
        self.state: ra2yr.GameState = ra2yr.GameState()
        self.type_classes: List[ra2yr.ObjectTypeClass] = []
        self.callbacks = []
        self.client: DualClient = DualClient(address, port)
        self._main_task = asyncio.create_task(log_exceptions(self.mainloop()))
        self.state_updated = asyncio.Condition()
        self._stop = asyncio.Event()
        # default callbacks

    def start(self):
        self.client.connect()

    async def stop(self):
        self._stop.set()
        await self._main_task
        await self.client.stop()

    def add_callback(self, fn):
        self.callbacks.append(fn)

    async def step(self, s: ra2yr.GameState):
        pass

    async def on_state_update(self, s: ra2yr.GameState):
        if not self.type_classes and self.state.current_frame > 0:
            res_tc = await self.run(commands_yr.GetTypeClasses())
            self.type_classes = res_tc.result.classes
            assert len(self.type_classes) > 0

        await self.step(s)

    async def get_state(self) -> ra2yr.GameState:
        cmd = commands_yr.GetGameState()
        state = await self.client.exec_command(cmd, timeout=5.0)
        state.result.Unpack(cmd)
        return cmd.result.state

    async def run_command(self, c: Any) -> core.CommandResult:
        return await self.client.exec_command(c)

    async def run(self, c: Any = None, **kwargs) -> Any:
        for k, v in kwargs.items():
            if isinstance(v, list):
                getattr(c.args, k).extend(v)
            else:
                try:
                    setattr(c.args, k, v)
                except:  # FIXME: more explicit check
                    getattr(c.args, k).CopyFrom(v)
        res = await self.run_command(c)
        res_o = type(c)()
        res.result.Unpack(res_o)
        return res_o

    async def mainloop(self):
        while not self._stop.is_set():
            try:
                s = await self.get_state()
                await self.on_state_update(s)
                self.state = s
                async with self.state_updated:
                    self.state_updated.notify_all()
            except asyncio.exceptions.TimeoutError:
                error("Couldn't fetch result")

            await asyncio.sleep(1 / self.poll_frequency)

    async def wait_state(self, cond, timeout=30):
        async with self.state_updated:
            await asyncio.wait_for(
                self.state_updated.wait_for(cond),
                timeout,
            )


class StateUtil:
    def __init__(self, type_classes: List[ra2yr.TypeClass]):
        self.type_classes = type_classes
        self.state = None

    def set_state(self, state: ra2yr.GameState):
        self.state = state

    def get_house(self, name: str) -> ra2yr.House:
        return next(h for h in self.state.houses if h.name == name)

    def get_units(self, house_name: str) -> ra2yr.Object:
        h = self.get_house(house_name)
        return (u for u in self.state.objects if u.pointer_house == h.self)

    def get_production(self, house_name: str) -> ra2yr.Factory:
        h = self.get_house(house_name)
        return (f for f in self.state.factories if f.owner == h.self)


class ManagerUtil:
    def __init__(self, manager: Manager):
        self.manager = manager

    def make_command(self, c: Any, **kwargs):
        for k, v in kwargs.items():
            if isinstance(v, list):
                getattr(c.args, k).extend(v)
            else:
                try:
                    setattr(c.args, k, v)
                except:  # FIXME: more explicit check
                    getattr(c.args, k).CopyFrom(v)
        return c

    def add_event(
        self,
        event_type: ra2yr.NetworkEvent = None,
        house_index: int = 0,
        **kwargs,
    ):
        return self.make_command(
            commands_yr.AddEvent(),
            event=ra2yr.Event(
                event_type=event_type, house_index=house_index, **kwargs
            ),
        )

    def mission_clicked(
        self,
        object_addresses=List[int],
        event=ra2yr.Mission,
        coordinates=None,
    ):
        return self.make_command(
            commands_yr.MissionClicked(),
            object_addresses=object_addresses,
            event=event,
            coordinates=coordinates,
        )

    def click_event(
        self,
        event_type: ra2yr.NetworkEvent = None,
        object_addresses: List[int] = None,
        **kwargs,
    ):
        return self.make_command(
            commands_yr.ClickEvent(),
            object_addresses=object_addresses,
            event=event_type,
        )

    def unit_command(
        self,
        object_addresses: List[int] = None,
        action: commands_yr.UnitAction = None,
    ):
        return self.make_command(
            commands_yr.UnitCommand(),
            object_addresses=object_addresses,
            action=action,
        )

    async def select(
        self,
        object_addresses: List[int] = None,
    ):
        return await self.manager.run(
            self.unit_command(
                object_addresses=object_addresses,
                action=commands_yr.ACTION_SELECT,
            )
        )

    async def move(self, object_addresses: List[int] = None, coordinates=None):
        return await self.manager.run(
            self.mission_clicked(
                object_addresses=object_addresses,
                event=ra2yr.Mission_Move,
                coordinates=coordinates,
            )
        )

    async def deploy(self, object_address: int = None):
        return await self.manager.run(
            self.click_event(
                event_type=ra2yr.NETWORK_EVENT_Deploy,
                object_addresses=[object_address],
            )
        )

    async def place_query(
        self, type_class: int = None, house_class: int = None, coordinates=None
    ) -> commands_yr.PlaceQuery:
        return await self.manager.run(
            self.make_command(
                commands_yr.PlaceQuery(),
                type_class=type_class,
                house_class=house_class,
                coordinates=coordinates,
            )
        )

    async def place_building(
        self, heap_id=None, is_naval=None, location=None
    ) -> commands_yr.AddEvent:
        return await self.manager.run(
            self.add_event(
                event_type=ra2yr.NETWORK_EVENT_Place,
                place=ra2yr.Event.Place(
                    rtti_type=ra2yr.ABSTRACT_TYPE_BUILDINGTYPE,
                    heap_id=heap_id,
                    is_naval=is_naval,
                    location=location,
                ),
            )
        )

    async def sell_building(self, object_address=None):
        return await self.manager.run(
            self.make_command(
                commands_yr.ClickEvent(),
                object_addresses=[object_address],
                event=ra2yr.NETWORK_EVENT_Sell,
            )
        )

    async def produce(self, house_index=0, rtti_id: int = 0, heap_id: int = 0):
        return await self.manager.run(
            self.add_event(
                event_type=0xE,
                house_index=house_index,
                production=ra2yr.Event.Production(
                    rtti_id=rtti_id, heap_id=heap_id
                ),
            )
        )

    async def produce_building(self, house_index: int = 0, heap_id: int = 0):
        # TODO: add to protobuf enums
        return await self.produce(
            house_index=house_index,
            rtti_id=ra2yr.ABSTRACT_TYPE_BUILDINGTYPE,
            heap_id=heap_id,
        )

    async def add_message(
        self,
        message: str = None,
        duration_frames: int = None,
        color: ra2yr.ColorScheme = None,
    ):
        return await self.manager.run(
            self.make_command(
                commands_yr.AddMessage(),
                message=message,
                duration_frames=duration_frames,
                color=color,
            )
        )

    async def read_value(self, **kwargs):
        return await self.manager.run(
            commands_yr.ReadValue(),
            data=commands_yr.ReadValue.Target(**kwargs),
        )

    async def inspect_configuration(self):
        return await self.manager.run(commands_yr.InspectConfiguration())

    async def get_system_state(self):
        return await self.manager.run(commands_builtin.GetSystemState())

    async def can_build_this(self, typeclass_id: int = 0) -> bool:
        pass

    async def get_place_locations(
        self, coords, type_class_id: int, house_pointer: int, rx: int, ry: int
    ):
        # get potential place locations
        place_query_grid = []
        for i in range(0, rx):
            for j in range(0, ry):
                place_query_grid.append(
                    tuple2coord(
                        tuple(
                            x + y * 256
                            for x, y in zip(coords, (i - rx, j - ry, 0))
                        )
                    )
                )

        res = await self.place_query(
            type_class=type_class_id,
            house_class=house_pointer,
            coordinates=place_query_grid,
        )
        return res
