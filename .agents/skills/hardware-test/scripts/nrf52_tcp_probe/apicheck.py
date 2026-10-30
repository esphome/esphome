"""apicheck.py ADDR: connect to the native API (plaintext) over Thread and print the device name and version."""

import argparse
import asyncio

from aioesphomeapi import APIClient


async def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("addr")
    args = parser.parse_args()
    client = APIClient(args.addr, 6053, None)
    await asyncio.wait_for(client.connect(login=True), 60)
    info = await client.device_info()
    print("API OK", info.name, info.esphome_version)
    await client.disconnect()


asyncio.run(main())
