"""ookdrive.py PEERHOST DUTHOST OUT [ROUNDS|probe]: async serial OOK NEC codes both ways through remote_transmitter
and remote_receiver on two ookcommon.yaml boards, with the power_down and begin_tx checks on the DUT (#22636).

Writes all log lines to OUT and prints one line per case. `probe` sends 3 codes each way, for setting up the pair.
API_KEY as in apiconn.py, the same key as apikey.txt.
"""

import asyncio
import pathlib
import re
import sys
import time

from aioesphomeapi import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text  # noqa: E402

PEERH, DUTH, OUT = sys.argv[1:4]
ARG = sys.argv[4] if len(sys.argv) > 4 else "1"
out = pathlib.Path(OUT).open("w", encoding="utf-8", buffering=1)  # noqa: SIM115
NEC = re.compile(r"NEC addr=0x1234 cmd=0x([0-9A-F]{4})")


class Dev:
    def __init__(self, host: str, tag: str) -> None:
        self.host, self.tag, self.codes, self.lines = host, tag, [], []

    async def start(self) -> None:
        self.cli = await connect(self.host)
        _, svcs = await self.cli.list_entities_services()
        self.svc = {s.name: s for s in svcs}
        self.cli.subscribe_logs(self.on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)

    def on_log(self, m) -> None:
        line = log_text(m)
        out.write(f"{time.strftime('%H:%M:%S')} {self.tag} {line}\n")
        if r := NEC.search(line):
            self.codes.append(int(r.group(1), 16))
        if "cct" in line or "[W]" in line or "[E]" in line:
            self.lines.append(line)

    async def act(self, name: str, **kw) -> None:
        await self.cli.execute_service(self.svc[name], kw)
        await asyncio.sleep(0.3)

    def count(self, mark: str) -> int:
        return sum(mark in line for line in self.lines)


async def wait_mark(dev: Dev, mark: str, before: int, timeout: float = 30) -> None:
    t = time.monotonic()
    while dev.count(mark) <= before and time.monotonic() - t < timeout:
        await asyncio.sleep(0.1)
    if dev.count(mark) <= before:
        # Stop: the case would otherwise report the previous mark as this run's evidence.
        raise SystemExit(f"INVALID: {dev.tag} never logged {mark.strip()}")


async def case(
    a: Dev,
    b: Dev,
    label: str,
    count: int,
    base: int,
    gap: int = 400,
    pre: str | None = None,
) -> None:
    b.codes.clear()
    n = a.count("SENT ")
    if pre:
        await a.act(pre)
        await asyncio.sleep(0.5)
    await a.act("send", count=count, base=base, gap_ms=gap)
    await wait_mark(a, "SENT ", n, timeout=count * (gap / 1000 + 0.2) + 10)
    await asyncio.sleep(1)
    got = set(b.codes) & set(range(base, base + count))
    sent = [line for line in a.lines if "SENT " in line][-1:]
    tail = sent[0][sent[0].find("SENT") :] if sent else "no SENT line"
    print(
        f"{label:42} sent={count} got={len(got)} frames={len(b.codes)} {tail}",
        flush=True,
    )


async def main() -> None:
    peer, dut = Dev(PEERH, "PEER"), Dev(DUTH, "DUT")
    await asyncio.gather(peer.start(), dut.start())
    await asyncio.sleep(1)
    if ARG == "probe":
        await dut.act("state")
        await peer.act("state")
        await case(dut, peer, "DUT -> PEER", 3, 1, 500)
        await case(peer, dut, "PEER -> DUT", 3, 2, 500)
    else:
        for r in range(int(ARG)):
            o = r * 0x100
            await case(
                dut, peer, "DUT -> PEER (begin_tx via on_transmit)", 10, 0x10 + o
            )
            await case(peer, dut, "PEER -> DUT (after begin_rx)", 10, 0x30 + o)
            await case(
                dut, peer, "DUT power_down, then DUT -> PEER", 10, 0x50 + o, pre="pdown"
            )
            await case(peer, dut, "PEER -> DUT after that", 10, 0x70 + o)
        for o in (0x100, 0x200):
            peer.codes.clear()
            for i in range(10):
                await dut.act("pdsend", cmd=0xD0 + o + i)
                await asyncio.sleep(0.9)
            await asyncio.sleep(1)
            got = set(peer.codes) & set(range(0xD0 + o, 0xDA + o))
            label = "power_down then begin_tx (raw), 1 code x10"
            print(
                f"{label:42} sent=10 got={len(got)} frames={len(peer.codes)}",
                flush=True,
            )
            await case(peer, dut, "PEER -> DUT after that", 10, 0xF0 + o)
        for _ in range(3):
            n = dut.count("PDTX")
            await dut.act("pdtx")
            await wait_mark(dut, "PDTX", n, 10)
        n = dut.count("REGCHK")
        await dut.act("regchk")
        await wait_mark(dut, "REGCHK", n, 10)
        await case(dut, peer, "DUT -> PEER after regchk", 10, 0x90)
        await case(peer, dut, "PEER -> DUT after regchk", 10, 0xB0)
    marks = ("REGCHK", "PDTX", "PDSENT", "STATE", "[W]", "[E]")
    for d in (peer, dut):
        print(
            f"{d.tag} lines:",
            *[x for x in d.lines if any(m in x for m in marks)][:12],
            sep="\n  ",
            flush=True,
        )
        await d.cli.disconnect()


asyncio.run(main())
