import argparse
import asyncio
import logging
import struct
import aiohttp
from aiohttp import web


class TCPClient:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.reader = None
        self.writer = None
        self.__fmt = "<I"
        self.__fmt_pack = "<I"

    def pack_length(self, length: int):
        return struct.pack(self.__fmt_pack, length)

    async def connect(self):
        self.reader, self.writer = await asyncio.open_connection(
            self.host, self.port
        )

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


async def websocket_handler(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)

    args = request.app["args"]
    tcp_conn = TCPClient(args.destination, args.port)
    await tcp_conn.connect()

    async for msg in ws:
        if msg.type == aiohttp.WSMsgType.BINARY:
            if msg.data == "close":
                await ws.close()
            elif len(msg.data) > 0:
                logging.debug("send msg, len=%d", len(msg.data))
                await tcp_conn.send_message(msg.data)
                ack = await tcp_conn.read_message()
                await ws.send_bytes(ack)
        elif msg.type == aiohttp.WSMsgType.ERROR:
            logging.error(
                "ws connection closed with exception %s", ws.exception()
            )
        else:
            logging.error("invalid type")

    await tcp_conn.aclose()
    logging.info("websocket connection closed")
    return ws


def parse_args():
    a = argparse.ArgumentParser(
        description="ra2yrcpp websocket proxy",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    a.add_argument(
        "-d", "--destination", default="127.0.0.1", help="destination address"
    )
    a.add_argument("-w", "--web-port", default=14520, help="webserver port")
    a.add_argument("-p", "--port", default=14521, help="destination port")
    return a.parse_args()


def main():
    # pylint: disable=unused-variable
    args = parse_args()
    app = web.Application()
    app.add_routes([web.get("/ws", websocket_handler)])
    app["args"] = args

    web.run_app(app, port=args.web_port)


if __name__ == "__main__":
    main()
