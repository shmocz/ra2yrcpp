import asyncio


class AsyncDict:
    def __init__(self):
        self._data = {}
        self._cond = asyncio.Condition()

    async def get_item(self, key, timeout: float = None, remove: bool = False):
        async with self._cond:
            await asyncio.wait_for(
                self._cond.wait_for(lambda: key in self._data), timeout
            )
            item = self._data[key]
            if remove:
                self._data.pop(key)
            return item

    async def put_item(self, key, value):
        async with self._cond:
            if key in self._data:
                raise RuntimeError(f"key {key} exists")
            self._data[key] = value
            self._cond.notify_all()
