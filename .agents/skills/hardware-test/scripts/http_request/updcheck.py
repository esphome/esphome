"""Run N update checks over the API and print the update state plus http_request/update/long-operation log lines.
Usage: updcheck.py IP LABEL N"""

import asyncio
import re
import sys
import time

import aioesphomeapi

IP, LABEL, N = sys.argv[1], sys.argv[2], int(sys.argv[3])


async def main():
    c = aioesphomeapi.APIClient(IP, 6053, None)
    await c.connect(login=True)
    info = await c.device_info()
    print(
        f"{LABEL} device {info.name} {info.esphome_version} {info.compilation_time}",
        flush=True,
    )
    ents, _ = await c.list_entities_services()
    upd = next(e for e in ents if isinstance(e, aioesphomeapi.UpdateInfo))
    states = []

    def on_state(s):
        if s.key == upd.key:
            states.append(s)
            print(
                f"{LABEL} {time.monotonic() - t0:7.2f} UPDATE in_progress={s.in_progress} current={s.current_version} latest={s.latest_version} title={s.title!r}",
                flush=True,
            )

    def on_log(m):
        t = (
            m.message.decode(errors="replace")
            if isinstance(m.message, bytes)
            else m.message
        )
        t = re.sub(r"\x1b\[[0-9;]*m", "", t)
        if re.search(
            r"http_request|update|took a long time|Guru|abort|heap|Free",
            t,
            re.IGNORECASE,
        ):
            print(f"{LABEL} {time.monotonic() - t0:7.2f} {t[:200]}", flush=True)

    t0 = time.monotonic()
    c.subscribe_states(on_state)
    c.subscribe_logs(on_log, log_level=aioesphomeapi.LogLevel.LOG_LEVEL_DEBUG)
    await asyncio.sleep(3)
    missing = []
    for i in range(N):
        print(f"{LABEL} {time.monotonic() - t0:7.2f} CHECK {i + 1}", flush=True)
        before = len(states)
        c.update_command(upd.key, aioesphomeapi.UpdateCommand.CHECK)
        await asyncio.sleep(25)
        if not any(not st.in_progress for st in states[before:]):
            print(f"{LABEL} CHECK {i + 1} got no finished update state", flush=True)
            missing.append(i + 1)
    await c.disconnect()
    print(f"{LABEL} {N - len(missing)} of {N} checks reported a state", flush=True)
    return 1 if missing else 0


sys.exit(asyncio.run(main()))
