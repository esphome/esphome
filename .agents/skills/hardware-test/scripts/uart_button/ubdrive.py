"""Press each uart button 3 times over the native API and save the device log, then summarize TX and RX per press.

usage: ubdrive.py HOST OUT   (plaintext API; matches uart-button-common.yaml)

OUT gets the press markers and the device's log lines. Diff `grep -E "PRESS|<<<|>>>" OUT` between dev and the PR; an
RX line can land after the next press marker when the log stream lags, which is not a difference in the bytes.
"""

import argparse
import asyncio
import pathlib

from aioesphomeapi import APIClient, ButtonInfo

BUTTONS = ("str", "bytes", "same str", "long")


async def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("host")
    parser.add_argument("out")
    args = parser.parse_args()
    client = APIClient(args.host, 6053, None)
    await client.connect(login=True)
    entities, _ = await client.list_entities_services()
    keys = {e.name: e.key for e in entities if isinstance(e, ButtonInfo)}
    lines: list[str] = []
    client.subscribe_logs(
        lambda m: lines.append(m.message.decode(errors="replace")), log_level=5
    )
    await asyncio.sleep(2)
    for r in range(3):
        for name in BUTTONS:
            lines.append(f"PRESS {r} {name}")
            client.button_command(keys[name])
            await asyncio.sleep(1.5)
    await asyncio.sleep(1)
    await client.disconnect()
    with pathlib.Path(args.out).open("w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    tx = sum(">>>" in line for line in lines)
    rx = sum("<<<" in line for line in lines)
    print(f"presses {3 * len(BUTTONS)} tx {tx} rx {rx}")


asyncio.run(main())
