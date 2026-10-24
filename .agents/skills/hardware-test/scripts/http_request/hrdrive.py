"""Drive the http_request action test device over the API and collect its hrt log lines.
Usage: hrdrive.py DEVICE_IP SERVER_BASE LABEL [https]"""

import asyncio
import os
import re
import sys
import time

import aioesphomeapi

IP, BASE, LABEL = sys.argv[1], sys.argv[2], sys.argv[3]
HTTPS = len(sys.argv) > 4
CASES = [
    ("get", "fast", BASE + "/fast"),
    ("get", "slow2s", BASE + "/slow?d=2"),
    ("get", "big70k", BASE + "/big?n=70000&rate=20000"),
    ("get", "chunked70k", BASE + "/chunked?n=70000&rate=20000"),
    ("post", "post500", BASE + "/post"),
    ("get", "redirect", BASE + "/redirect"),
    ("get", "hang", BASE + "/hang?d=20"),
    ("get", "unreachable", "http://192.0.2.1:81/"),
]
if HTTPS:
    CASES += [
        ("get", "https_get", "https://www.howsmyssl.com/a/check"),
        ("post", "https_post", "https://httpbin.org/post"),
    ]

ONLY = os.environ.get("ONLY")
if ONLY:
    CASES = [c for c in CASES if c[1] in ONLY.split(",")]
ROUNDS = int(os.environ.get("ROUNDS", "2"))
EXTRAS = not ONLY


async def main():
    c = aioesphomeapi.APIClient(IP, 6053, None)
    await c.connect(login=True)
    _, services = await c.list_entities_services()
    svc = {s.name: s for s in services}
    lines = []

    def on_log(m):
        t = (
            m.message.decode(errors="replace")
            if isinstance(m.message, bytes)
            else m.message
        )
        t = re.sub(r"\x1b\[[0-9;]*m", "", t)
        if (
            "[hrt" in t
            or "http_request" in t
            or "Script" in t
            or "already running" in t
        ):
            lines.append(t)
            print(f"{LABEL} {t}", flush=True)

    c.subscribe_logs(on_log, log_level=aioesphomeapi.LogLevel.LOG_LEVEL_DEBUG)
    await asyncio.sleep(1)
    for rnd in range(1, ROUNDS + 1):
        for kind, name, url in CASES:
            n0 = len(lines)
            print(f"{LABEL} == {name} round {rnd}", flush=True)
            if kind == "get":
                await c.execute_service(svc["req_get"], {"url": url})
            else:
                await c.execute_service(
                    svc["req_post"], {"url": url, "payload": "x" * 500}
                )
            end = time.monotonic() + 40
            while time.monotonic() < end and not any(
                "RESULT" in ln for ln in lines[n0:]
            ):
                await asyncio.sleep(0.2)
            await asyncio.sleep(1.5)
        if not EXTRAS:
            continue
        print(f"{LABEL} == burst round {rnd}", flush=True)
        n0 = len(lines)
        await c.execute_service(svc["burst"], {"url": BASE + "/slow?d=1", "count": 6})
        end = time.monotonic() + 30
        while (
            time.monotonic() < end
            and sum(("BURSTRESP" in ln or "BURSTERR" in ln) for ln in lines[n0:]) < 6
        ):
            await asyncio.sleep(0.2)
        await asyncio.sleep(2)
        print(f"{LABEL} == cancel round {rnd}", flush=True)
        await c.execute_service(svc["cancel"], {"url": BASE + "/slow?d=2"})
        await asyncio.sleep(9)
        print(f"{LABEL} == cross round {rnd}", flush=True)
        n0 = len(lines)
        await c.execute_service(svc["req_get"], {"url": BASE + "/slow?d=2&tag=cross"})
        await c.execute_service(
            svc["burst"], {"url": BASE + "/slow?d=1&tag=crossburst", "count": 3}
        )
        end = time.monotonic() + 30
        while time.monotonic() < end and (
            sum("BURSTRESP" in ln or "BURSTERR" in ln for ln in lines[n0:]) < 3
            or not any("RESULT" in ln for ln in lines[n0:])
        ):
            await asyncio.sleep(0.2)
        await asyncio.sleep(2)
    await c.disconnect()


asyncio.run(main())
