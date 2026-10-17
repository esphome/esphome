"""Drive the online_image cases over the native API and judge each from the serial capture."""

import asyncio
import pathlib
import re
import sys
import time

from aioesphomeapi import APIClient

HOST, KEY, CAPTURE, BASE, MODE = sys.argv[1:6]
CAP_S = 45

CASES = [
    # (label, action, path, expected outcome)
    ("ok bmp", "fetch", "ok.bmp", "finished"),
    ("ok png", "fetch", "ok.png", "finished"),
    ("ok jpg", "fetch", "ok.jpg", "finished"),
    ("ok qoi", "fetch", "ok.qoi", "finished"),
    ("truncated bmp", "fetch", "truncated.bmp", "error"),
    ("ok bmp after error", "fetch", "ok.bmp", "finished"),
    ("large header, 256 byte buffer", "fetch_small", "large_header.bmp", "error"),
    ("ok bmp, 256 byte buffer", "fetch_small", "ok.bmp", "finished"),
    ("empty body", "fetch", "empty.bmp", "error"),
    ("stalled connection", "fetch", "stall.bmp", "error"),
    ("closed early", "fetch", "closed_early.bmp", "error"),
    ("slow but alive", "fetch", "slow.bmp", "finished"),
    ("chunked bmp", "fetch", "chunked.bmp", "finished"),
    ("chunked png", "fetch", "chunked.png", "finished"),
    ("chunked qoi", "fetch", "chunked.qoi", "finished"),
    ("chunked bmp cut", "fetch", "chunked_cut.bmp", "error"),
    ("chunked png cut", "fetch", "chunked_cut.png", "any"),
    ("no content-length", "fetch", "noclen.bmp", "any"),
    ("huge jpeg claim", "fetch", "huge.jpg", "error"),
    ("ok jpg after huge", "fetch", "ok.jpg", "finished"),
    ("ok bmp again", "fetch", "ok.bmp", "finished"),
    ("ok bmp refetch: 304", "refetch", "", "finished"),
    ("ok png", "fetch", "ok.png", "finished"),
    ("ok png refetch: 304", "refetch", "", "finished"),
    ("chunked bmp", "fetch", "chunked.bmp", "finished"),
    ("chunked bmp refetch: 304", "refetch", "", "finished"),
    ("chunked png", "fetch", "chunked.png", "finished"),
    ("chunked png refetch: not cached", "refetch", "", "finished"),
    ("chunked png cut", "fetch", "chunked_cut.png", "any"),
    ("chunked png cut refetch: not cached", "refetch", "", "any"),
]
if MODE == "base":
    # Each stuck case is followed by a fetch that shows the symptom, then a reboot.
    CASES = [
        ("ok bmp", "fetch", "ok.bmp", "finished"),
        ("truncated bmp", "fetch", "truncated.bmp", "error"),
        ("ok bmp after truncated", "fetch", "ok.bmp", "finished"),
        ("REBOOT", "", "", ""),
        ("large header, 256 byte buffer", "fetch_small", "large_header.bmp", "error"),
        ("ok bmp, 256 byte buffer", "fetch_small", "ok.bmp", "finished"),
        ("REBOOT", "", "", ""),
        ("empty body", "fetch", "empty.bmp", "error"),
        ("ok bmp after empty", "fetch", "ok.bmp", "finished"),
        ("REBOOT", "", "", ""),
        ("stalled connection", "fetch", "stall.bmp", "error"),
        ("ok bmp after stall", "fetch", "ok.bmp", "finished"),
        ("REBOOT", "", "", ""),
        ("closed early", "fetch", "closed_early.bmp", "error"),
        ("ok bmp after closed early", "fetch", "ok.bmp", "finished"),
        ("REBOOT", "", "", ""),
        ("chunked png", "fetch", "chunked.png", "finished"),
        ("ok bmp after chunked png", "fetch", "ok.bmp", "finished"),
        ("REBOOT", "", "", ""),
        ("chunked bmp cut", "fetch", "chunked_cut.bmp", "error"),
        ("ok bmp after chunked cut", "fetch", "ok.bmp", "finished"),
        ("REBOOT", "", "", ""),
        ("huge jpeg claim", "fetch", "huge.jpg", "error"),
        ("ok jpg after huge", "fetch", "ok.jpg", "finished"),
    ]

OUTCOME = re.compile(r"download (finished|error): (img_small|img)")
INTEREST = re.compile(
    r"\[(E|W)\]\[online_image|Image already being updated|download (finished|error)|Downloading image|Updating image|Image fully downloaded|HTTP 304|allocat|Panic|abort\(\)|Guru|Backtrace|rst:0x",
    re.IGNORECASE,
)


def tail(path: str, offset: int) -> tuple[list[str], int]:
    with pathlib.Path(path).open("rb") as f:
        f.seek(offset)
        data = f.read()
    return data.decode("utf-8", "replace").splitlines(), offset + len(data)


async def connect() -> tuple[APIClient, dict, int]:
    for _ in range(60):
        try:
            c = APIClient(HOST, 6053, None, noise_psk=KEY)
            await c.connect(login=True)
            _, services = await c.list_entities_services()
            ents, _ = await c.list_entities_services()
            btn = next(e.key for e in ents if getattr(e, "object_id", "") == "reboot")
            return c, {s.name: s for s in services}, btn
        except Exception as exc:  # noqa: BLE001
            print(f"connect: {exc}", flush=True)
            await asyncio.sleep(3)
    raise SystemExit("API never connected")


async def main() -> None:
    client, actions, btn = await connect()
    with pathlib.Path(CAPTURE).open("rb") as f:
        f.seek(0, 2)
        offset = f.tell()
    rows = []
    for label, action, path, expect in CASES:
        if label == "REBOOT":
            client.button_command(btn)
            await asyncio.sleep(1)
            await client.disconnect()
            await asyncio.sleep(8)
            client, actions, btn = await connect()
            _, offset = tail(CAPTURE, offset)
            print("--- rebooted", flush=True)
            continue
        img = "img_small" if action.endswith("_small") else "img"
        t0 = time.monotonic()
        args = {} if action.startswith("refetch") else {"url": f"{BASE}/{path}"}
        await client.execute_service(actions[action], args)
        outcome, notes = "no outcome", []
        while time.monotonic() - t0 < CAP_S:
            await asyncio.sleep(0.5)
            lines, offset = tail(CAPTURE, offset)
            done = False
            for line in lines:
                if INTEREST.search(line):
                    notes.append(line.strip())
                m = OUTCOME.search(line)
                if m and m.group(2) == img:
                    outcome = m.group(1)
                    done = True
            if done:
                break
        took = time.monotonic() - t0
        if outcome == "no outcome":
            took = CAP_S
            # drain a little more so the "already being updated" line lands
            await asyncio.sleep(1)
            lines, offset = tail(CAPTURE, offset)
            notes += [line.strip() for line in lines if INTEREST.search(line)]
        ok = expect in ("any", outcome)
        rows.append((label, path, expect, outcome, took, ok, notes))
        print(
            f"{'PASS' if ok else 'FAIL'} {label:32s} {path:18s} expected {expect:9s} got {outcome:10s} {took:5.1f}s",
            flush=True,
        )
        for n in notes:
            print(f"      {n}", flush=True)
        await asyncio.sleep(1.5)
    await client.disconnect()
    print(
        "\nSUMMARY",
        "all expectations met"
        if all(r[5] for r in rows)
        else "MISMATCHES: " + ", ".join(r[0] for r in rows if not r[5]),
        flush=True,
    )


asyncio.run(main())
