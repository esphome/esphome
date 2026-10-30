"""Press each uart button 3 times over the native API and save the device log, then summarize TX and RX per press.

usage: ubdrive.py HOST OUT   (plaintext API; matches uart-button-common.yaml)

OUT gets the press markers and the device's log lines. Diff `grep -E "PRESS|<<<|>>>" OUT` between dev and the PR; an
RX line can land after the next press marker when the log stream lags, which is not a difference in the bytes, so
the payloads are checked over the whole log. Exits 2 when a TX or RX line is missing or does not match the configured
payload of its button.
"""

import argparse
import asyncio
import pathlib
import re
import sys

from aioesphomeapi import APIClient, ButtonInfo

BUTTONS = ("str", "bytes", "same str", "long")
LONG = b"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!?"
PAYLOADS = {
    "str": b"Hello UART",
    "bytes": bytes([0x01, 0x02, 0xFF, 0x00, 0x7E]),
    "same str": b"Hello UART",
    "long": LONG,
}


def hexline(data: bytes) -> str:
    return ":".join(f"{b:02X}" for b in data)


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
    await asyncio.sleep(3)  # the last RX line can trail the log stream
    await client.disconnect()
    with pathlib.Path(args.out).open("w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    plain = [re.sub(r"\x1b\[[0-9;]*m", "", line) for line in lines]
    tx = [line.split(">>> ", 1)[1].strip() for line in plain if ">>> " in line]
    rx = [line.split("<<< ", 1)[1].strip() for line in plain if "<<< " in line]
    expected = [hexline(PAYLOADS[name]) for _ in range(3) for name in BUTTONS]
    print(f"presses {len(expected)} tx {len(tx)} rx {len(rx)}")
    # TX lines come in press order; an RX line may trail the next press marker but keeps its order
    bad = [
        f"{side} {i}: got {got or 'nothing'}, expected {want}"
        for side, seen in (("TX", tx), ("RX", rx))
        for i, want in enumerate(expected)
        if (got := (seen[i] if i < len(seen) else "")) != want
    ]
    for line in bad:
        print(line)
    if bad or len(tx) != len(expected) or len(rx) != len(expected):
        print(f"FAIL: expected {len(expected)} matching TX and RX payloads")
        sys.exit(2)
    print("PASS")


asyncio.run(main())
