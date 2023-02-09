import asyncio
import datetime
import logging
import struct
import traceback
from typing import Any, Dict

import aiohttp
from ra2yrproto import core

from .async_container import AsyncDict

debug = logging.debug


async def log_exceptions(coro):
    try:
        return await coro
    except Exception:
        logging.error("%s", traceback.format_exc())


class TCPClient:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.reader = None
        self.writer = None
        self.__fmt = "<I"
        self.__fmt_pack = "<I"
        self._retry_delay = 1.0
        self._timeout = 30

    def pack_length(self, length: int):
        return struct.pack(self.__fmt_pack, length)

    async def _try_connect(self):
        return await asyncio.open_connection(self.host, self.port)

    async def connect(self):
        t = datetime.datetime.now().timestamp()
        d = 0
        while d < self._timeout:
            try:
                self.reader, self.writer = await self._try_connect()
                return
            except Exception:
                logging.warning(
                    "connect failed, retrying (timeout in %ds)",
                    round(self._timeout - d),
                )
                await asyncio.sleep(self._retry_delay)
            d = datetime.datetime.now().timestamp() - t

    async def aclose(self):
        self.writer.close()
        await self.writer.wait_closed()

    async def read_message(self) -> bytes:
        # read message length
        data = await self.reader.read(struct.calcsize(self.__fmt))
        message_length = struct.unpack(self.__fmt, data)[0]

        # read the actual message
        r = message_length
        res = bytearray()
        while r > 0:
            chunk_bytes = await self.reader.read(message_length)
            res.extend(chunk_bytes)
            r -= len(chunk_bytes)
        return bytes(res)

    async def send_message(self, m: str | bytes):
        # write length
        data = m
        if not isinstance(m, bytes):
            data = m.encode()
        self.writer.write(self.pack_length(len(data)))
        # write actual message
        self.writer.write(data)
        await self.writer.drain()


class WebSocketClient:
    def __init__(self, uri: str, timeout=5.0):
        self.uri = uri
        self.in_queue = asyncio.Queue()
        self.out_queue = asyncio.Queue()
        self.timeout = timeout
        self.task = None
        self._tries = 15
        self._connect_delay = 1.0

    def open(self):
        self.task = asyncio.create_task(log_exceptions(self.main()))

    async def close(self):
        await self.in_queue.put(None)
        await self.task

    async def send_message(self, m: str) -> aiohttp.WSMessage:
        await self.in_queue.put(m)
        return await self.out_queue.get()

    async def main(self):
        # send the initial message
        msg = await self.in_queue.get()
        for i in range(self._tries):
            try:
                debug("connect, try %d %d", i, self._tries)
                await self._main_session(msg)
                break
            except asyncio.exceptions.CancelledError:
                break
            except Exception:
                logging.warning(
                    "connect failed (try %d/%d)", i + 1, self._tries
                )
                if i + 1 == self._tries:
                    raise
                await asyncio.sleep(self._connect_delay)

    async def _main_session(self, msg):
        async with aiohttp.ClientSession() as session:
            debug("connecting to %s %s msg %s", self.uri, session, msg)
            async with session.ws_connect(self.uri, autoclose=False) as ws:
                debug("connected to %s", self.uri)
                await ws.send_bytes(msg)

                async for msg in ws:
                    await self.out_queue.put(msg)
                    in_msg = await self.in_queue.get()
                    if in_msg is None:
                        await ws.close()
                        break
                    await ws.send_bytes(in_msg)
            self.in_queue = None
            self.out_queue = None
            debug("close _main_session")


class DualClient:
    def __init__(self, host: str, port: int, timeout: float = 5.0):
        self.host = host
        self.port = port
        self.conns: Dict[str, WebSocketClient] = {}
        self.uri = f"http://{host}:{port}/ws"
        self.queue_id = -1
        self.timeout = timeout
        self.results = AsyncDict()
        self.in_queue = asyncio.Queue()
        self._poll_task = None
        self._stop = asyncio.Event()
        # FIXME: ugly
        self._queue_set = asyncio.Event()

    def connect(self):
        for k in ["command", "poll"]:
            self.conns[k] = WebSocketClient(self.uri, self.timeout)
            self.conns[k].open()
            debug("opened %s", k)
        self._poll_task = asyncio.create_task(log_exceptions(self._poll_loop()))

    def make_command(self, msg=None, command_type=None) -> core.Command:
        c = core.Command()
        c.command_type = command_type
        c.command.Pack(msg)
        return c

    def make_poll_blocking(self, queue_id, timeout) -> core.Command:
        c = core.PollResults()
        c.args.queue_id = queue_id
        c.args.timeout = timeout
        return self.make_command(c, core.POLL_BLOCKING)

    def parse_response(self, msg: str) -> core.Response:
        res = core.Response()
        res.ParseFromString(msg)
        return res

    async def run_client_command(self, c: Any) -> core.RunCommandAck:
        msg = await self.conns["command"].send_message(
            self.make_command(c, core.CLIENT_COMMAND).SerializeToString()
        )

        res = self.parse_response(msg.data)
        ack = core.RunCommandAck()
        res.body.Unpack(ack)
        return ack

    # TODO: could wrap this into task and cancel at exit
    async def exec_command(self, c: Any, timeout=None):
        msg = await self.run_client_command(c)
        if self.queue_id < 0:
            self.queue_id = msg.queue_id
            self._queue_set.set()
        # wait until results polled
        return await self.results.get_item(msg.id, timeout=timeout, remove=True)

    async def _poll_loop(self):
        await self._queue_set.wait()
        while not self._stop.is_set():
            msg = await self.conns["poll"].send_message(
                self.make_poll_blocking(
                    self.queue_id, int(self.timeout * 1000)
                ).SerializeToString()
            )
            res = self.parse_response(msg.data)
            cc = core.PollResults()
            res.body.Unpack(cc)
            for x in cc.result.results:
                await self.results.put_item(x.command_id, x)

    async def stop(self):
        self._stop.set()
        await self._poll_task
        await self.conns["command"].close()
        await self.conns["poll"].close()
