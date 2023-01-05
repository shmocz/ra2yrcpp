import asyncio
import logging
from typing import Any, List, Iterator

from ra2yrproto import commands_yr, core, ra2yr


from google.protobuf.internal.decoder import _DecodeVarint32

from pyra2yr.network import DualClient, log_exceptions

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
        self.type_classes: List[ra2yr.ObjectTypeClass] = None
        self.callbacks = []
        self.client: DualClient = DualClient(address, port)
        self._main_task = asyncio.create_task(log_exceptions(self.mainloop()))
        self.state_updated = asyncio.Condition()
        self._stop = asyncio.Event()

    def start(self):
        self.client.connect()

    async def stop(self):
        self._stop.set()
        await self._main_task
        await self.client.stop()

    def add_callback(self, fn):
        self.callbacks.append(fn)

    async def on_state_update(self, s: ra2yr.GameState):
        for c in self.callbacks:
            c(s)

    async def get_state(self) -> ra2yr.GameState:
        cmd = commands_yr.GetGameState()
        state = await self.client.exec_command(cmd, timeout=5.0)
        state.result.Unpack(cmd)
        return cmd.result.state

    async def run_command(self, c: Any) -> core.CommandResult:
        return await self.client.exec_command(c)

    async def run(self, c: Any, **kwargs) -> Any:
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
        await self.run_command(commands_yr.CreateHooks())
        await self.run_command(commands_yr.CreateCallbacks())

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
