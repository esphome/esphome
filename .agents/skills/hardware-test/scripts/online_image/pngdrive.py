"""Fetch images through the online_image API actions and report outcome, time, loop time maximum and heap minimum per case.
usage: [CASES=png|noise|end|bmpqoi|short|jpeg] pngdrive.py HOST KEY LOGFILE BASEURL LABEL
"""

import asyncio
import contextlib
import os
import pathlib
import re
import sys
import time
import urllib.error
import urllib.request

from aioesphomeapi import APIClient, LogLevel, SensorInfo, SensorState, UserService

HOST, KEY, LOGFILE, BASE, LABEL = sys.argv[1:6]
CAP_S = 60
OUTCOME = re.compile(r"download (finished|error): (\w+)")
INTEREST = re.compile(
    r"took a long time|Error|error|Download ended|No data received|Waiting for more|Decoded|cut off|already being|headers do not fit|buffer_size|Downloading image|fully downloaded|allocat|not caching"
)

PNG = [
    ("gradient 320x240 png", "fetch", "gradient.png", "finished"),
    ("gradient again", "fetch", "gradient.png", "finished"),
    ("gradient again", "fetch", "gradient.png", "finished"),
    ("flat 640x480 png", "fetch", "flat.png", "finished"),
    ("flat again", "fetch", "flat.png", "finished"),
    ("flat again", "fetch", "flat.png", "finished"),
    ("gradient jpeg", "fetch", "gradient.jpg", "finished"),
    ("ok bmp 16x16", "fetch", "ok.bmp", "finished"),
    ("ok png 16x16 body = gradient", "fetch", "ok.png", "finished"),
    ("chunked png", "fetch", "chunked.png", "finished"),
    ("chunked png cut in half", "fetch", "chunked_cut.png", "any"),
    ("closed early png", "fetch", "closed_early.png", "error"),
    ("stalled png", "fetch", "stall.png", "error"),
    ("slow png", "fetch", "slow.png", "finished"),
    ("empty png", "fetch", "empty.png", "error"),
    ("no content-length png", "fetch", "noclen.png", "any"),
    ("truncated bmp", "fetch", "truncated.bmp", "error"),
    ("gradient after errors", "fetch", "gradient.png", "finished"),
    ("gradient refetch 304", "refetch", "", "finished"),
    ("noise 320x240 png", "fetch", "noise.png", "finished"),
    ("noise again", "fetch", "noise.png", "finished"),
]

NOISE = [("noise 320x240 png", "fetch", "noise.png", "finished")] * 3

END = [
    (f"{pre}{img}", "fetch", f"{pre}{img}", exp)
    for img in ("flat.png", "gradient.png")
    for pre, exp in (
        ("", "finished"),
        ("noclen_", "finished"),
        ("chunked_", "finished"),
        ("half_", "any"),
        ("stall_", "error"),
    )
] + [("gradient after", "fetch", "gradient.png", "finished")]

BMPQOI = [
    (f"{pre}{img}", "fetch", f"{pre}{img}", exp)
    for img in (
        "bmp1_640.bmp",
        "bmp8_320.bmp",
        "bmp24_320.bmp",
        "qoi_flat.qoi",
        "qoi_check.qoi",
        "qoi_grad.qoi",
        "qoi_noise.qoi",
    )
    for pre, exp in (
        ("", "finished"),
        ("", "finished"),
        ("noclen_", "any"),
        ("chunked_", "any"),
        ("half_", "error"),
    )
] + [
    ("ok bmp 16x16", "fetch", "ok.bmp", "finished"),
    ("ok qoi 16x16", "fetch", "ok.qoi", "finished"),
    ("truncated bmp", "fetch", "truncated.bmp", "error"),
    ("stall_bmp1_640.bmp", "fetch", "stall_bmp1_640.bmp", "error"),
    ("stall_qoi_flat.qoi", "fetch", "stall_qoi_flat.qoi", "error"),
    ("qoi_flat after", "fetch", "qoi_flat.qoi", "finished"),
]

SHORT = [
    ("gradient 320x240 png", "fetch", "gradient.png", "finished"),
    ("gradient again", "fetch", "gradient.png", "finished"),
    ("noise 320x240 png", "fetch", "noise.png", "finished"),
    ("noise again", "fetch", "noise.png", "finished"),
    ("noise again", "fetch", "noise.png", "finished"),
    ("gradient after noise", "fetch", "gradient.png", "finished"),
]

JPEG = [
    ("noise 320x240 jpeg 69 KB", "fetch", "noise320.jpg", "finished"),
    ("noise 320x240 again", "fetch", "noise320.jpg", "finished"),
    ("noise 320x240 again", "fetch", "noise320.jpg", "finished"),
    ("noise 640x480 jpeg 233 KB", "fetch", "noise640.jpg", "finished"),
    ("noise 640x480 again", "fetch", "noise640.jpg", "finished"),
    ("gradient 640x480 jpeg 14 KB", "fetch", "gradient640.jpg", "finished"),
    ("4 KB buffer: noise 320x240", "fetch_small", "noise320.jpg", "finished"),
    ("4 KB buffer: noise 640x480", "fetch_small", "noise640.jpg", "finished"),
    ("4 KB buffer: 12 KB exif", "fetch_small", "bigexif320.jpg", "any"),
    ("64 KB buffer: 12 KB exif", "fetch", "bigexif320.jpg", "finished"),
    ("progressive jpeg", "fetch", "progressive320.jpg", "any"),
    ("no content-length", "fetch", "noclen_noise320.jpg", "finished"),
    ("chunked", "fetch", "chunked_noise320.jpg", "finished"),
    ("half with matching length", "fetch", "half_noise320.jpg", "error"),
    ("closed early", "fetch", "closed_early.jpg", "error"),
    ("stalled after a third", "fetch", "stall_noise320.jpg", "error"),
    ("empty", "fetch", "empty.jpg", "error"),
    ("noise 320x240 after errors", "fetch", "noise320.jpg", "finished"),
    ("refetch 304", "refetch", "", "finished"),
    ("ok png 16x16", "fetch", "ok.png", "finished"),
    ("ok bmp 16x16", "fetch", "ok.bmp", "finished"),
    ("4 KB buffer: noise 640x480 again", "fetch_small", "noise640.jpg", "finished"),
    ("4 KB buffer: half", "fetch_small", "half_noise320.jpg", "error"),
    ("4 KB buffer: stalled", "fetch_small", "stall_noise320.jpg", "error"),
    (
        "4 KB buffer: noise 320x240 after errors",
        "fetch_small",
        "noise320.jpg",
        "finished",
    ),
]

CASE_SETS = {
    "png": PNG,
    "noise": NOISE,
    "end": END,
    "bmpqoi": BMPQOI,
    "short": SHORT,
    "jpeg": JPEG,
}
CASES = CASE_SETS[os.environ.get("CASES", "png")]


def missing_base_files() -> list[str]:
    """Base files behind prefixed cases that the server would answer with 404.

    A 404 ends as a download error on the device, which would pass the half_ and stall_ cases untested.
    """
    builtin = {"chunked_cut", "closed_early", "large_header"}
    missing = []
    for _, _, path, _ in CASES:
        stem = path.rsplit(".", 1)[0]
        prefix, _, rest = path.partition("_")
        if (
            stem in builtin
            or prefix not in ("half", "noclen", "chunked", "stall")
            or not rest
        ):
            continue
        try:
            with urllib.request.urlopen(f"{BASE}/{rest}", timeout=10):
                pass
        except urllib.error.HTTPError as exc:
            if exc.code == 404:
                missing.append(rest)
    return sorted(set(missing))


async def main() -> int:
    if missing := missing_base_files():
        print(
            f"server has no {', '.join(missing)}; run the image generators first",
            flush=True,
        )
        return 2
    c = APIClient(HOST, 6053, None, noise_psk=KEY)
    await c.connect(login=True)
    ents, services = await c.list_entities_services()
    actions = {s.name: s for s in services if isinstance(s, UserService)}
    loop_key = next(
        e.key for e in ents if isinstance(e, SensorInfo) and e.name == "Loop Time"
    )
    heap_key = next(
        e.key for e in ents if isinstance(e, SensorInfo) and e.name == "Heap Free"
    )
    state = {"loop": [], "heap": None, "heap_min": None, "lines": []}

    def on_state(s) -> None:
        if isinstance(s, SensorState) and s.key == loop_key:
            state["loop"].append(s.state)
        elif isinstance(s, SensorState) and s.key == heap_key:
            state["heap"] = s.state
            if state["heap_min"] is None or s.state < state["heap_min"]:
                state["heap_min"] = s.state

    def on_log(msg) -> None:
        line = msg.message.decode(errors="replace")
        log.write(f"{time.strftime('%H:%M:%S')} {line}\n")
        state["lines"].append(line)

    with pathlib.Path(LOGFILE).open("w", encoding="utf-8") as log:
        c.subscribe_states(on_state)
        c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
        await asyncio.sleep(3)
        print(f"{LABEL}: heap at start {state['heap']}")
        rows = []
        for label, action, path, expect in CASES:
            img = "img_small" if action.endswith("_small") else "img"
            state["loop"].clear()
            state["lines"].clear()
            state["heap_min"] = None
            args = {} if action.startswith("refetch") else {"url": f"{BASE}/{path}"}
            t0 = time.monotonic()
            log.write(f">>> {label} {path}\n")
            try:
                await c.execute_service(actions[action], args)
            except Exception as exc:  # noqa: BLE001
                print(f"      api call failed ({exc!r}); reconnecting", flush=True)
                with contextlib.suppress(Exception):
                    await c.disconnect()
                await asyncio.sleep(10)
                await c.connect(login=True)
                ents, services = await c.list_entities_services()
                actions = {s.name: s for s in services if isinstance(s, UserService)}
                c.subscribe_states(on_state)
                c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
                await asyncio.sleep(2)
                state["lines"].clear()
                t0 = time.monotonic()
                await c.execute_service(actions[action], args)
            outcome = "no outcome"
            while time.monotonic() - t0 < CAP_S:
                await asyncio.sleep(0.25)
                for line in state["lines"]:
                    m = OUTCOME.search(line)
                    if m and m.group(2) == img:
                        outcome = m.group(1)
                if outcome != "no outcome":
                    break
            took = time.monotonic() - t0
            await asyncio.sleep(4.5)  # two loop time reports after the case
            notes = [
                ln for ln in state["lines"] if INTEREST.search(ln) and "Loop" not in ln
            ]
            loop_max = max(state["loop"]) if state["loop"] else float("nan")
            ok = outcome == expect or (expect == "any" and outcome != "no outcome")
            rows.append((label, path, expect, outcome, took, loop_max, ok))
            print(
                f"{'PASS' if ok else 'FAIL'} {label:30s} {path:18s} expected {expect:9s} got {outcome:10s} {took:5.1f}s loop max {loop_max:7.0f} ms heap min {state['heap_min']}",
                flush=True,
            )
            for n in notes[:8]:
                print(f"      {n.strip()}", flush=True)
        await c.disconnect()
    print(
        "\nSUMMARY",
        "all expectations met"
        if all(r[6] for r in rows)
        else "MISMATCHES: " + ", ".join(r[0] for r in rows if not r[6]),
    )
    return 0 if all(r[6] for r in rows) else 1


sys.exit(asyncio.run(main()))
