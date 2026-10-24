"""Run the tx_all script N times over the API and count what each sent frame decoded as.
Usage: rbdrive.py IP N"""

import asyncio
import collections
import re
import sys

import aioesphomeapi

IP, N = sys.argv[1], int(sys.argv[2])
lines: list[str] = []


async def main() -> None:
    c = aioesphomeapi.APIClient(IP, 6053, None)
    await c.connect(login=True)
    i = await c.device_info()
    print("device", i.name, i.compilation_time, flush=True)
    _, svcs = await c.list_entities_services()
    s = {x.name: x for x in svcs}

    def on_log(m):
        lines.append(re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace")))

    c.subscribe_logs(on_log, log_level=aioesphomeapi.LogLevel.LOG_LEVEL_DEBUG)
    await asyncio.sleep(1)
    complete = 0
    for rnd in range(1, N + 1):
        start = len(lines)
        await c.execute_service(s["tx_all"], {})
        for _ in range(200):
            if any("tx done" in x for x in lines[start:]):
                complete += 1
                break
            await asyncio.sleep(0.1)
        else:
            print(f"round {rnd}: no tx done within 20 s", flush=True)
        await asyncio.sleep(1)
    await c.disconnect()
    # per sent protocol: what was decoded
    table: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    cur = None
    for x in lines:
        if m := re.search(r"\]: tx (\w+)$", x):
            cur = m[1]
            continue
        if cur is None or cur == "done":
            continue
        if m := re.search(r"Received ([A-Za-z0-9_ ]+?):", x):
            table[cur][m[1]] += 1
        elif "ext dumper saw" in x:
            table[cur]["ext"] += 1
    print(f"{complete} of {N} rounds complete", flush=True)
    for proto, cnt in table.items():
        print(
            f"{proto:11s} " + ", ".join(f"{k}={v}" for k, v in sorted(cnt.items())),
            flush=True,
        )
    for x in lines:
        if "ext dumper saw" not in x and re.search(
            r"rbt|error|Error|overflow|Guru|panic", x
        ):
            print("  ", x[:160])


asyncio.run(main())
