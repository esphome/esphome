"""ccdrive.py TXHOST RXHOST MODE OUT [TXLABEL RXLABEL]: drives both radios over API actions and counts the packets
each receiver logs. Writes all log lines to OUT and prints one summary line per case.

MODE: short, long, r2long, r2mid, lambda, r2lambda, fast (fast38, fast100 for one rate), and the dev bug repros crc,
rxtx, txback, txrx, txset and nocrc64 (see recipes/cc1101.md)."""

import asyncio
import pathlib
import re
import statistics
import sys
import time

from aioesphomeapi import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text  # noqa: E402

TXH, RXH, MODE, OUT = sys.argv[1:5]
TXN, RXN = (sys.argv[5:7] + ["TX", "RX"][len(sys.argv[5:7]) :])[:2]
out = pathlib.Path(OUT).open("w", encoding="utf-8", buffering=1)  # noqa: SIM115
RX_RE = re.compile(
    r"RX len=(\d+) b0=(\d+) seq=(\d+) ok=(\d) rssi=(-?[\d.]+) lqi=(\d+) fo=(-?[\d.]+)"
)


class Dev:
    def __init__(self, host, tag):
        self.host, self.tag, self.rx, self.lines = host, tag, [], []
        self.failed = (
            0  # actions that could not be sent and CFG/RATE lines that never came
        )

    async def start(self):
        self.cli = await connect(self.host)
        _, svcs = await self.cli.list_entities_services()
        self.svc = {s.name: s for s in svcs}
        self.cli.subscribe_logs(self.on_log, log_level=LogLevel.LOG_LEVEL_INFO)

    def on_log(self, m):
        line = log_text(m)
        out.write(f"{time.strftime('%H:%M:%S')} {self.tag} {line}\n")
        if r := RX_RE.search(line):
            self.rx.append(tuple(float(v) for v in r.groups()))
        if "cct" in line or "cc1101" in line:
            self.lines.append(line)

    async def act(self, name, **kw):
        try:
            await self.cli.execute_service(self.svc[name], kw)
        except Exception as e:  # noqa: BLE001
            self.failed += 1
            print(
                f"  ACTION FAILED {self.tag} {name}: {type(e).__name__}: {e}",
                flush=True,
            )
        await asyncio.sleep(0.3)


async def wait_mark(dev, mark, before, timeout=8.0):
    t = time.monotonic()
    while (
        sum(1 for line in dev.lines if mark in line) <= before
        and time.monotonic() - t < timeout
    ):
        await asyncio.sleep(0.1)
    if sum(1 for line in dev.lines if mark in line) <= before:
        dev.failed += 1
        print(f"  INVALID: {dev.tag} never logged {mark.strip()}", flush=True)
        return False
    return True


REPORTED = 0  # failures already shown on a result line


def verdict(tx, rx):
    """INVALID when an action or an awaited log line failed since the last result line."""
    global REPORTED
    total = tx.failed + rx.failed
    bad, REPORTED = total > REPORTED, total
    return "INVALID " if bad else ""


async def configure(dev, length, crc=False):
    n = sum(1 for line in dev.lines if "CFG " in line)
    await dev.act("cfg", len=length, crc=crc)
    await wait_mark(dev, "CFG ", n)


def sent_count(dev):
    return sum(1 for line in dev.lines if "SENT " in line)


async def tx_errors(tx, before):
    """The errors= of the sender's SENT line for the last send, or ? when it never came."""
    if not await wait_mark(tx, "SENT ", before):
        return "?"
    m = re.search(
        r"SENT .* errors=(\d+)", [line for line in tx.lines if "SENT " in line][-1]
    )
    return m[1] if m else "?"


async def case(
    tx,
    rx,
    label,
    count,
    length,
    b0,
    gap_ms,
    rx_len=None,
    tx_len=None,
    crc=False,
    wait=None,
):
    nr, nt = (
        sum(1 for line in rx.lines if "CFG " in line),
        sum(1 for line in tx.lines if "CFG " in line),
    )
    if rx_len != -1:
        await rx.act("cfg", len=rx_len if rx_len is not None else length, crc=crc)
    if tx_len != -1:
        await tx.act(
            "cfg", len=tx_len if tx_len is not None else min(length, 64), crc=crc
        )
    if rx_len != -1:
        await wait_mark(rx, "CFG ", nr)
    if tx_len != -1:
        await wait_mark(tx, "CFG ", nt)
    await asyncio.sleep(0.5)
    rx.rx.clear()
    ns = sent_count(tx)
    await tx.act("send", count=count, len=length, b0=b0, gap_ms=gap_ms)
    await asyncio.sleep(
        wait if wait is not None else count * (gap_ms / 1000 + length / 600 + 0.15) + 2
    )
    txerr = await tx_errors(tx, ns)
    got = [p for p in rx.rx if int(p[1]) == (b0 & 0xFF)]
    good = [p for p in got if int(p[0]) == length and p[3] == 1]
    other = len(rx.rx) - len(got)

    def stat(i):
        return f"{statistics.mean(p[i] for p in good):.1f}" if good else "-"

    lens = sorted({int(p[0]) for p in got})
    print(
        f"{label:34} {verdict(tx, rx)}sent={count} txerr={txerr} good={len(good)} wrong={len(got) - len(good)} lens={lens} other={other} "
        f"rssi={stat(4)} lqi={stat(5)} fo={stat(6)}",
        flush=True,
    )
    return len(good)


async def main():
    tx, rx = Dev(TXH, "TX"), Dev(RXH, "RX")
    await asyncio.gather(tx.start(), rx.start())
    await asyncio.sleep(1)
    if MODE.startswith("fast"):
        rates = {
            "fast38": (38400, 1, 20000, 135000, 50),
            "fast100": (100000, 0, 50000, 325000, 20),
        }
        if MODE not in (*rates, "fast"):
            sys.exit(f"unknown mode {MODE}")
        for key in [MODE] if MODE in rates else list(rates):
            sr, mod, dev, bw, poll = rates[key]
            for d in (tx, rx):
                n = sum(1 for line in d.lines if "RATE " in line)
                await d.act("rate", sr=sr, mod=mod, dev=dev, bw=bw, poll=poll, sync=3)
                await wait_mark(d, "RATE ", n)
            await asyncio.sleep(0.5)
            print(
                "RATE",
                [line.split("RATE ")[1] for line in rx.lines if "RATE " in line][-1:],
                flush=True,
            )
            k = sr / 4800
            await case(tx, rx, f"{key} len=32", 20, 32, 70, 100)
            await case(
                tx,
                rx,
                f"{key} len=300",
                20,
                300,
                71,
                100,
                wait=20 * (0.1 + 0.3 + 300 / 600 / k) + 3,
            )
            await case(
                tx,
                rx,
                f"{key} len=1024",
                10,
                1024,
                72,
                100,
                wait=10 * (0.1 + 0.4 + 1024 / 600 / k) + 3,
            )
            await case(tx, rx, f"{key} burst 300 x5", 5, 300, 73, 0, wait=8)
            ov_before = len([line for line in rx.lines if "overflow" in line])
            print(f"{key} RX overflow warnings so far: {ov_before}", flush=True)
        for d in (tx, rx):
            await d.act("rate", sr=4800, mod=1, dev=47607, bw=203000, poll=300, sync=2)
    elif MODE == "short":
        for direction, a, b in ((f"{TXN}->{RXN}", tx, rx), (f"{RXN}->{TXN}", rx, tx)):
            for length in (32, 64):
                for crc in (False, True):
                    await case(
                        a,
                        b,
                        f"{direction} len={length} crc={int(crc)}",
                        20,
                        length,
                        1,
                        150,
                        crc=crc,
                    )
    elif MODE == "long":
        await case(tx, rx, f"{TXN}->{RXN} len=32 (runtime, long build)", 10, 32, 1, 200)
        for length in (100, 255, 300, 1024):
            await case(tx, rx, f"{TXN}->{RXN} long len={length}", 10, length, 2, 300)
        await rx.act("idle_rx")
        await case(tx, rx, "after set_idle+begin_rx len=100", 5, 100, 3, 300, wait=None)
        await rx.act("tx_rx")
        await case(tx, rx, "after begin_tx+begin_rx len=300", 5, 300, 4, 300)
    elif MODE == "r2long":
        await case(tx, rx, "burst 300 x5, no gap", 5, 300, 10, 0)
        await case(tx, rx, "burst 65 x10, no gap", 10, 65, 11, 0)
        await case(tx, rx, "burst 1024 x3, no gap", 3, 1024, 12, 0)
        # power down in the middle of a run of 300-byte packets, wake 1 s later
        await configure(rx, 300)
        await configure(tx, 64)
        rx.rx.clear()
        ns = sent_count(tx)
        await tx.act("send", count=10, len=300, b0=13, gap_ms=300)
        await asyncio.sleep(1.6)
        await rx.act("pdown")
        await asyncio.sleep(1.0)
        await rx.act("wup")
        await asyncio.sleep(10 * 0.85 + 2)
        got = [p for p in rx.rx if int(p[1]) == 13]
        good = sum(1 for p in got if int(p[0]) == 300 and p[3] == 1)
        txerr = await tx_errors(tx, ns)
        print(
            f"{'power_down mid-run, wake 1 s later':34} {verdict(tx, rx)}sent=10 txerr={txerr} good={good} "
            f"wrong={len(got) - good}",
            flush=True,
        )
        await case(tx, rx, "after power_down/wake_up 300", 5, 300, 14, 300, rx_len=-1)
        await rx.act("pdown")
        await asyncio.sleep(0.5)
        await rx.act("wup")
        await case(tx, rx, "power_down/wake_up between, 1024", 3, 1024, 15, 300)
    elif MODE == "r2mid":
        await configure(rx, 1024)
        await configure(tx, 64)
        await asyncio.sleep(0.5)
        await tx.act("send", count=1, len=1024, b0=40, gap_ms=0)
        await asyncio.sleep(0.6)
        await rx.act("hf")
        await rx.act("setlen", len=32)
        await asyncio.sleep(0.6)
        await rx.act("hf")
        await asyncio.sleep(1.5)
        await case(
            tx, rx, "after setlen 32 mid-packet, len 32", 5, 32, 41, 200, rx_len=-1
        )
        await configure(rx, 1024)
        await tx.act("send", count=1, len=1024, b0=42, gap_ms=0)
        await asyncio.sleep(0.6)
        await rx.act("hf")
        await rx.act("pmode", enable=False)
        await asyncio.sleep(0.6)
        await rx.act("hf")
        await rx.act("pmode", enable=True)
        await asyncio.sleep(1.5)
        await case(
            tx, rx, "after packet_mode off/on mid-packet, len 100", 5, 100, 43, 300
        )
        await rx.act("pdown")
        await rx.act("setlen", len=300)
        await case(
            tx,
            rx,
            "asleep after setlen 300 (expect 0)",
            3,
            300,
            44,
            300,
            rx_len=-1,
            wait=5,
        )
        await rx.act("wup")
        await case(tx, rx, "after wake_up, len 300", 5, 300, 45, 300, rx_len=-1)
        hf = [line for line in rx.lines if "HF " in line]
        print("HF lines:", [line.split("HF ")[1][:2] for line in hf], flush=True)
    elif MODE == "txset":
        await case(tx, rx, "32 before", 10, 32, 60, 150)
        await rx.act("tx_setters")
        await case(tx, rx, "begin_tx, setters, begin_rx", 20, 32, 61, 150, rx_len=-1)
        print(
            "lines:",
            [
                line.split("TX_SETTERS ")[1][:60]
                for line in rx.lines
                if "TX_SETTERS" in line
            ],
            flush=True,
        )
    elif MODE == "txback":
        await case(tx, rx, f"{TXN}->{RXN} before", 10, 32, 50, 150)
        await case(
            rx,
            tx,
            f"{RXN} send (plain) -> {TXN}",
            10,
            32,
            51,
            150,
            rx_len=-1,
            tx_len=-1,
        )
        await case(
            tx, rx, f"{TXN}->{RXN} after plain sends", 20, 32, 52, 150, rx_len=-1
        )
        await rx.act("tx_only")
        await case(
            rx,
            tx,
            f"begin_tx, then {RXN} send -> {TXN}",
            10,
            32,
            53,
            150,
            rx_len=-1,
            tx_len=-1,
        )
        await case(tx, rx, f"{TXN}->{RXN} after that", 20, 32, 54, 150, rx_len=-1)
    elif MODE == "nocrc64":
        await case(tx, rx, "64 no CRC x100", 100, 64, 55, 150)
        await case(tx, rx, "64 CRC x100", 100, 64, 56, 150, crc=True)
    elif MODE == "r2lambda":
        await case(tx, rx, "lambda 40 before", 5, 40, 39, 200, rx_len=-1, tx_len=40)
        await rx.act("setlen", len=100)
        await case(
            tx, rx, "runtime setlen 100 (clears lambda)", 5, 100, 16, 300, rx_len=-1
        )
        await rx.act("setlen", len=32)
        await case(tx, rx, "runtime setlen 32", 5, 32, 17, 200, rx_len=-1, tx_len=32)
        await rx.act("pmode", enable=False)
        await rx.act("pmode", enable=True)
        await case(
            tx,
            rx,
            "after packet_mode off/on, len 32",
            5,
            32,
            18,
            200,
            rx_len=-1,
            tx_len=32,
        )
        await rx.act("setlen", len=300)
        await case(tx, rx, "runtime setlen 300", 5, 300, 19, 300, rx_len=-1)
    elif MODE == "crc":
        await case(tx, rx, "64 CRC", 20, 64, 20, 150, crc=True)
        await case(tx, rx, "32 CRC", 20, 32, 21, 150, crc=True)
        await case(tx, rx, "64 no CRC", 20, 64, 22, 150)
        await tx.act("whiten", enable=True)
        await case(
            tx, rx, "64 CRC, TX whitening only (bad)", 5, 64, 23, 200, crc=True, wait=4
        )
        await tx.act("whiten", enable=False)
        await case(tx, rx, "64 CRC after the bad ones", 10, 64, 24, 150, crc=True)
    elif MODE == "rxtx":
        await case(tx, rx, "32 before", 10, 32, 30, 150)
        await rx.act("tx_rx")
        await case(tx, rx, "begin_tx + begin_rx", 20, 32, 31, 150, rx_len=-1)
        await rx.act("tx_idle_rx")
        await case(tx, rx, "begin_tx + set_idle + begin_rx", 20, 32, 32, 150, rx_len=-1)
        await rx.act("tx_reset")
        await case(tx, rx, "begin_tx + reset", 20, 32, 33, 150, rx_len=-1)
        await rx.act("tx_only")
        await case(
            rx,
            tx,
            f"begin_tx, then {RXN} send -> {TXN}",
            10,
            32,
            34,
            150,
            rx_len=-1,
            tx_len=-1,
        )
        await case(tx, rx, f"{TXN}->{RXN} after that", 20, 32, 35, 150, rx_len=-1)
    elif MODE == "txrx":
        await case(tx, rx, "len=32 before", 5, 32, 6, 200)
        await rx.act("tx_rx")
        await case(tx, rx, "len=32 after begin_tx+begin_rx", 5, 32, 7, 200, rx_len=-1)
        await rx.act("idle_rx")
        await case(tx, rx, "len=32 after set_idle+begin_rx", 5, 32, 8, 200, rx_len=-1)
        await case(tx, rx, "len=32 after cfg", 5, 32, 9, 200)
    elif MODE == "lambda":
        for length in (10, 40, 64):
            await case(
                tx,
                rx,
                f"{TXN}->{RXN} lambda len={length}",
                10,
                length,
                length - 1,
                200,
                rx_len=-1,
                tx_len=length,
            )
        await case(
            tx,
            rx,
            f"{TXN}->{RXN} lambda discard (b0=0xEE)",
            3,
            40,
            0xEE,
            200,
            rx_len=-1,
            tx_len=40,
        )
        await case(
            tx,
            rx,
            f"{TXN}->{RXN} lambda after discard len=40",
            5,
            40,
            39,
            200,
            rx_len=-1,
            tx_len=40,
        )
        await rx.act("idle_rx")
        await case(
            tx,
            rx,
            "lambda after set_idle+begin_rx len=40",
            5,
            40,
            39,
            200,
            rx_len=-1,
            tx_len=40,
        )
    else:
        sys.exit(f"unknown mode {MODE}")
    for d in (tx, rx):
        warn = [line for line in d.lines if "[W]" in line or "[E]" in line]
        print(f"{d.tag} warnings/errors: {len(warn)}", *warn[:10], sep="\n  ")
        await d.cli.disconnect()
    if tx.failed or rx.failed:
        sys.exit(
            f"INVALID: {tx.failed + rx.failed} failed actions or missing CFG/RATE lines"
        )


asyncio.run(main())
