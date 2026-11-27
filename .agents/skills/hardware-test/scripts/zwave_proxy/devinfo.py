import asyncio
import sys

from aioesphomeapi import APIClient


async def main():
    c = APIClient(sys.argv[1], 6053, None)
    await c.connect(login=True)
    di = await c.device_info()
    print(
        "version",
        di.esphome_version,
        "compiled",
        di.compilation_time,
        "home_id",
        hex(di.zwave_home_id),
        "flags",
        di.zwave_proxy_feature_flags,
    )
    await c.disconnect()


asyncio.run(main())
