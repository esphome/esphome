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
# cases whose request must fail (past the 8 s timeout, or an address that never answers)
EXPECT_ERROR = {"hang", "unreachable"}
EXTRAS = not ONLY


timeouts: list[str] = []
mismatches: list[str] = []


async def main() -> int:
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

    def mismatch(what: str, why: str) -> None:
        print(f"{LABEL} {what} MISMATCH: {why}", flush=True)
        mismatches.append(what)

    async def wait_until(done, secs: float, what: str) -> None:
        """Poll done() until it holds; print a TIMEOUT marker when the deadline passes first."""
        end = time.monotonic() + secs
        while not done():
            if time.monotonic() >= end:
                print(f"{LABEL} {what} TIMEOUT", flush=True)
                timeouts.append(what)
                return
            await asyncio.sleep(0.2)

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
            await wait_until(
                lambda n0=n0: any("RESULT" in ln for ln in lines[n0:]),
                40,
                f"{name} round {rnd}",
            )
            result = next((ln for ln in lines[n0:] if "RESULT" in ln), None)
            if result is not None and (" error " in result) != (name in EXPECT_ERROR):
                mismatch(f"{name} round {rnd}", result)
            await asyncio.sleep(1.5)
        if not EXTRAS:
            continue
        print(f"{LABEL} == burst round {rnd}", flush=True)
        n0 = len(lines)
        await c.execute_service(svc["burst"], {"url": BASE + "/slow?d=1", "count": 6})
        await wait_until(
            lambda n0=n0: (
                sum("BURSTRESP" in ln or "BURSTERR" in ln for ln in lines[n0:]) >= 6
            ),
            30,
            f"burst round {rnd}",
        )
        if any("BURSTERR" in ln for ln in lines[n0:]):
            mismatch(f"burst round {rnd}", "a burst request failed")
        await asyncio.sleep(2)
        print(f"{LABEL} == cancel round {rnd}", flush=True)
        n0 = len(lines)
        await c.execute_service(svc["cancel"], {"url": BASE + "/slow?d=2"})
        await asyncio.sleep(9)
        if not any("CANCEL stopped running=0" in ln for ln in lines[n0:]):
            mismatch(
                f"cancel round {rnd}", "script.stop did not leave the script stopped"
            )
        print(f"{LABEL} == cross round {rnd}", flush=True)
        n0 = len(lines)
        await c.execute_service(svc["req_get"], {"url": BASE + "/slow?d=2&tag=cross"})
        await c.execute_service(
            svc["burst"], {"url": BASE + "/slow?d=1&tag=crossburst", "count": 3}
        )
        await wait_until(
            lambda n0=n0: (
                sum("BURSTRESP" in ln or "BURSTERR" in ln for ln in lines[n0:]) >= 3
                and any("RESULT" in ln for ln in lines[n0:])
            ),
            30,
            f"cross round {rnd}",
        )
        if any(
            "BURSTERR" in ln or ("RESULT" in ln and " error " in ln)
            for ln in lines[n0:]
        ):
            mismatch(f"cross round {rnd}", "a request failed")
        await asyncio.sleep(2)
    await c.disconnect()
    if timeouts:
        print(f"{LABEL} TIMEOUTS: {', '.join(timeouts)}", flush=True)
    if mismatches:
        print(f"{LABEL} MISMATCHES: {', '.join(mismatches)}", flush=True)
    return 1 if timeouts or mismatches else 0


sys.exit(asyncio.run(main()))
