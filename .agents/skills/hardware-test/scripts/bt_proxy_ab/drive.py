"""Bluetooth proxy A/B driver (aioesphomeapi). Writes JSON lines to OUT.

drive.py HOST adv OUT SECONDS              adverts per second, active then passive scan (scanner mode set over the API)
drive.py HOST disc OUT PEER ROUNDS MODE    MODE free (cold, 60 s budget, RTTs), retry (cold, 10 s budget, 5 attempts),
                                           cached (has_cache, 30 s budget, RTTs)
drive.py HOST third OUT ROUNDS VARIANT     VARIANT eve: hold L+M (cached), connect Eve cold;
                                           pd: hold L (cached) + fresh Eve (cached), connect M cold;
                                           L: hold M (cached) + fresh Eve (cached), connect L cold
drive.py HOST notify OUT SECONDS           hold L (cached), subscribe the 200 ms tick, count gaps and drops
drive.py HOST churn OUT CYCLES             cached connect/disconnect alternating L and M, slot accounting
drive.py HOST eve OUT ROUNDS               Eve Flare alone, cold, read as first command
L is PEER_L, M is PEER_M and Eve the HK_NAME device (see peers.py). env API_KEY.
For the device log over the API run ../ble_conn_interval/apilogcap.py alongside (suite.sh does).
"""

import asyncio
import contextlib
import json
import pathlib
import statistics
import sys
import time
from typing import NamedTuple

HERE = pathlib.Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parent)]  # peers.py here, apiconn.py one level up
from aioesphomeapi import BluetoothScannerMode  # noqa: E402
from apiconn import adv_name, connect  # noqa: E402
from peers import HK_NAME as EVE, L, M, addrs  # noqa: E402

# passive scans see no scan response, so the servers are also matched by address
ADDRS = {
    int(mac.replace(":", ""), 16): {"L": L, "M": M}[lb]
    for mac, lb in addrs().items()
    if lb in ("L", "M")
}
FF01 = "5ec2ff01-0000-1000-8000-00805f9b34fb"
FF02 = "5ec2ff02-0000-1000-8000-00805f9b34fb"
FF03 = "5ec2ff03-0000-1000-8000-00805f9b34fb"


HFILE = HERE / "handles.json"


class H(NamedTuple):
    """A characteristic by handle, with its CCCD handle (from a stored full discovery)."""

    handle: int
    cccd: int | None


def cccd(c) -> int | None:
    """Handle of the characteristic's 2902 descriptor."""
    return next(
        (d.handle for d in c.descriptors if d.uuid.lower().startswith("00002902")), None
    )


def save_handles(name: str, svcs) -> None:
    hs = json.loads(HFILE.read_text(encoding="utf-8")) if HFILE.exists() else {}
    new = {}
    for s in svcs.services:
        for c in s.characteristics:
            if c.uuid in (FF01, FF02, FF03):
                new[c.uuid] = [c.handle, cccd(c)]
    # keep the stored handles unless this discovery found every one of them (a partial table must not replace them)
    if FF01 in new and (name != L or len(new) == 3):
        hs[name] = new
        HFILE.write_text(json.dumps(hs), encoding="utf-8")


def find_chr(svcs, uuid, name=None):
    if svcs is None:
        if not HFILE.exists():
            return None
        h = json.loads(HFILE.read_text(encoding="utf-8")).get(name, {}).get(uuid)
        return H(*h) if h else None
    c = next(
        (c for s in svcs.services for c in s.characteristics if c.uuid == uuid), None
    )
    if c is None:
        return None
    return H(c.handle, cccd(c))


class D:
    def __init__(self, cli, out: str) -> None:
        self.cli = cli
        self.out = pathlib.Path(out).open("a", encoding="utf-8")  # noqa: SIM115
        self.peers: dict[str, tuple[int, int]] = {}
        self.adv = 0
        self.addrs: set[int] = set()
        self.st: dict[int, dict] = {}
        self.free = None
        self.scanner = None

    def rec(self, **kw) -> None:
        kw["ts"] = round(time.time(), 3)
        line = json.dumps(kw)
        self.out.write(line + "\n")
        self.out.flush()
        print(line, flush=True)

    async def start(self, want: list[str], wait: float = 60) -> None:
        info = await self.cli.device_info()
        self.flags = info.bluetooth_proxy_feature_flags_compat(self.cli.api_version)
        self.cli.subscribe_bluetooth_scanner_state(
            lambda m: setattr(self, "scanner", (m.state, m.mode))
        )
        self.cli.subscribe_bluetooth_connections_free(
            lambda f, lim, al: setattr(self, "free", (f, lim, list(al)))
        )

        def on_adv(m) -> None:
            self.adv += len(m.advertisements)
            for a in m.advertisements:
                self.addrs.add(a.address)
                n = adv_name(bytes(a.data)) or ADDRS.get(a.address)
                if n and n.startswith(EVE):
                    n = EVE
                if n and n in (
                    L,
                    M,
                    EVE,
                ):  # an unset PEER_M ("") must not match a nameless advertiser
                    self.peers[n] = (a.address, a.address_type)

        self.cli.subscribe_bluetooth_le_raw_advertisements(on_adv)
        end = time.monotonic() + wait
        while time.monotonic() < end and not all(w in self.peers for w in want):
            await asyncio.sleep(0.5)
        missing = [w for w in want if w not in self.peers]
        if missing:
            self.rec(result="not_seen", missing=missing)
            sys.exit(2)

    async def connect(self, name: str, cache: bool, timeout: float = 20) -> dict:
        addr, atype = self.peers[name]
        st = {"name": name, "lost": [], "first": None, "notify": None}
        ev = asyncio.Event()

        def on_state(c: bool, mtu: int, err: int) -> None:
            if st["first"] is None:
                st["first"] = (c, mtu, err, time.monotonic())
                ev.set()
            elif not c:
                st["lost"].append((err, time.monotonic()))

        st["t0"] = time.monotonic()
        try:
            st["cancel"] = await self.cli.bluetooth_device_connect(
                addr,
                on_state,
                timeout=timeout,
                feature_flags=self.flags,
                has_cache=cache,
                address_type=atype,
            )
        except Exception as e:  # noqa: BLE001
            st["exc"] = f"{type(e).__name__}: {e}"
        if st["first"] is not None:
            st["connected"], st["mtu"], st["err"], t1 = st["first"]
            st["t_conn"] = round(t1 - st["t0"], 3)
        else:
            st["connected"] = False
        if st["connected"]:
            self.st[addr] = st
        elif st.get(
            "cancel"
        ):  # a failed connect's state handler would keep getting later links' messages
            with contextlib.suppress(Exception):
                st["cancel"]()
        return st

    async def disconnect(self, name: str) -> None:
        addr = self.peers[name][0]
        st = self.st.pop(addr, None)
        try:
            await asyncio.wait_for(self.cli.bluetooth_device_disconnect(addr), 15)
        except Exception as e:  # noqa: BLE001
            # the proxy may still hold the slot; later slot errors are not the build's
            self.rec(
                result="disconnect_failed", name=name, exc=f"{type(e).__name__}: {e}"
            )
        if st and st.get("cancel"):
            st["cancel"]()

    async def drop_all(self) -> None:
        """Disconnect every peer still linked."""
        for n in (L, M, EVE):
            if n in self.peers and self.peers[n][0] in self.st:
                await self.disconnect(n)

    def held(self, name: str) -> bool:
        st = self.st.get(self.peers[name][0])
        return bool(st) and not st["lost"]

    async def clear(self, name: str) -> bool:
        for _ in range(4):
            try:
                r = await self.cli.bluetooth_device_clear_cache(
                    self.peers[name][0], timeout=10
                )
                if r.success:
                    return True
            except Exception as e:  # noqa: BLE001
                print(f"clear cache error: {e}", flush=True)
            await asyncio.sleep(3)
        return False

    async def hold(self, name: str) -> bool:
        if self.held(name):
            return True
        old = self.st.pop(self.peers[name][0], None)
        if old and old.get(
            "cancel"
        ):  # the lost link's handler must not see the next connect
            with contextlib.suppress(Exception):
                old["cancel"]()
        for _ in range(5):
            st = await self.connect(name, True)
            if st["connected"]:
                return True
            print(f"hold {name} failed: {st.get('err')} {st.get('exc')}", flush=True)
            await asyncio.sleep(3)
        return False

    async def rtts(self, name: str, svcs, n: int = 5) -> dict:
        """Median read, write-with-response and write-to-notify times (the server echoes an ff01 write on ff02)."""
        addr = self.peers[name][0]
        res = {}
        ch = find_chr(svcs, FF01, name)
        rd, wr, nt = [], [], []
        try:
            for _ in range(n):
                t = time.monotonic()
                v = await self.cli.bluetooth_gatt_read(addr, ch.handle, timeout=10)
                rd.append(time.monotonic() - t)
                if (
                    len(v) != 1
                ):  # ff01 holds one byte: a stored handle from an older server table
                    return {"harness": "stale_handles"}
            for i in range(n):
                t = time.monotonic()
                await self.cli.bluetooth_gatt_write(
                    addr, ch.handle, bytes([i]), True, timeout=10
                )
                wr.append(time.monotonic() - t)
            echo = find_chr(svcs, FF02, name)
            if echo is not None:
                q: asyncio.Queue = asyncio.Queue()
                stop, _rm = await self.cli.bluetooth_gatt_start_notify(
                    addr, echo.handle, lambda h, d: q.put_nowait(time.monotonic())
                )
                try:  # the subscription goes away even when an echo fails
                    await self.cli.bluetooth_gatt_write_descriptor(
                        addr, echo.cccd, b"\x01\x00", timeout=10
                    )
                    for i in range(n):
                        while not q.empty():
                            q.get_nowait()
                        t = time.monotonic()
                        await self.cli.bluetooth_gatt_write(
                            addr, ch.handle, bytes([0xA0 + i]), True, timeout=10
                        )
                        tr = await asyncio.wait_for(q.get(), 10)
                        nt.append(tr - t)
                finally:
                    with contextlib.suppress(Exception):
                        await stop()
        except Exception as e:  # noqa: BLE001
            res["rtt_exc"] = f"{type(e).__name__}: {e}"[:120]
            res["rtt_partial"] = True  # report.py counts these rounds apart
        if rd:
            res["read_ms"] = round(1000 * statistics.median(rd), 1)
        if wr:
            res["write_ms"] = round(1000 * statistics.median(wr), 1)
        if nt:
            res["notify_ms"] = round(1000 * statistics.median(nt), 1)
        return res

    async def disc_once(self, name: str, budget: float, cold: bool, rtt: bool) -> dict:
        if cold and not await self.clear(name):
            return {
                "connected": False,
                "exc": "clear cache failed",
                "harness": "clear_failed",
            }
        adv0 = self.adv
        st = await self.connect(name, not cold, timeout=budget)
        res = {k: st.get(k) for k in ("connected", "t_conn", "err", "exc", "mtu")}
        if not st["connected"]:
            return res
        addr = self.peers[name][0]
        try:
            svcs = None
            if cold:
                rem = budget - (time.monotonic() - st["t0"])
                svcs = await asyncio.wait_for(
                    self.cli.bluetooth_gatt_get_services(addr), max(rem, 0.1)
                )
                res["t_svc"] = round(time.monotonic() - st["t0"], 3)
                res["n"] = [
                    len(svcs.services),
                    sum(len(s.characteristics) for s in svcs.services),
                    sum(
                        len(c.descriptors)
                        for s in svcs.services
                        for c in s.characteristics
                    ),
                ]
                save_handles(name, svcs)
            ch = find_chr(svcs, FF01, name)
            if ch is None:  # wrong server or peer: a harness fault, not the build's
                res["harness"] = "no_ff01"
                await self.disconnect(name)
                return res
            rem = budget - (time.monotonic() - st["t0"])
            await asyncio.wait_for(
                self.cli.bluetooth_gatt_write(addr, ch.handle, b"\x01", True),
                max(rem, 0.1),
            )
            res["t_cmd"] = round(time.monotonic() - st["t0"], 3)
            if rtt:
                res.update(await self.rtts(name, svcs))
        except TimeoutError:
            res["timeout"] = round(time.monotonic() - st["t0"], 3)
        except Exception as e:  # noqa: BLE001
            res["exc"] = f"{type(e).__name__}: {e}"
            res["t_fail"] = round(time.monotonic() - st["t0"], 3)
        res["lost"] = [(e, round(t - st["t0"], 3)) for e, t in st["lost"]]
        res["adv_per_s"] = round(
            (self.adv - adv0) / max(time.monotonic() - st["t0"], 0.1), 1
        )
        await self.disconnect(name)
        return res


async def main() -> None:
    # a typo must not run some other scenario
    nargs = {"adv": 5, "disc": 7, "third": 6, "notify": 5, "churn": 5, "eve": 5}
    a = sys.argv
    if (
        len(a) < 3
        or nargs.get(a[2]) != len(a)
        or (a[2] == "disc" and a[6] not in ("free", "retry", "cached"))
        or (a[2] == "third" and a[5] not in ("eve", "pd", "L"))
    ):
        sys.exit(__doc__)
    # third and churn always use M, disc when asked to
    if not M and (a[2] in ("third", "churn") or (a[2] == "disc" and a[4] == "M")):
        sys.exit("set PEER_M to the local name of the M server")
    host, cmd, out = sys.argv[1], sys.argv[2], sys.argv[3]
    lost_api = asyncio.Event()

    async def on_stop(expected: bool) -> None:
        lost_api.set()

    cli = await connect(host, on_stop=on_stop)
    d = D(cli, out)

    def check_api() -> None:
        if lost_api.is_set():
            d.rec(result="api_lost")
            sys.exit(3)

    if cmd == "adv":
        secs = float(sys.argv[4])
        await d.start([], 1)
        not_applied = 0
        for mode in (BluetoothScannerMode.ACTIVE, BluetoothScannerMode.PASSIVE):
            cli.bluetooth_scanner_set_mode(mode)
            await asyncio.sleep(8)
            per = []
            d.addrs.clear()
            t_end = time.monotonic() + secs
            while time.monotonic() < t_end:
                c0 = d.adv
                await asyncio.sleep(10)
                per.append((d.adv - c0) / 10)
                check_api()
            applied = d.scanner is not None and d.scanner[1] == mode
            if (
                not applied
            ):  # the proxy ignored the request: these counts are not for this mode
                print(f"scanner mode {mode.name} not applied: {d.scanner}", flush=True)
                not_applied += 1
            d.rec(
                test="adv",
                mode=mode.name,
                applied=applied,
                scanner=str(d.scanner),
                mean=round(statistics.mean(per), 1),
                min10=min(per),
                max10=max(per),
                unique=len(d.addrs),
            )
        cli.bluetooth_scanner_set_mode(BluetoothScannerMode.ACTIVE)
        # the later steps assume an active scan: a proxy left passive would skew them all
        t = time.monotonic()
        while time.monotonic() - t < 10 and not (
            d.scanner is not None and d.scanner[1] == BluetoothScannerMode.ACTIVE
        ):
            await asyncio.sleep(0.5)
        if d.scanner is None or d.scanner[1] != BluetoothScannerMode.ACTIVE:
            d.rec(test="adv", result="restore_not_applied", scanner=str(d.scanner))
            sys.exit(1)
        if not_applied:  # the counts are not for the requested modes
            sys.exit(1)
    elif cmd == "disc":
        name, rounds, mode = sys.argv[4], int(sys.argv[5]), sys.argv[6]
        name = {"L": L, "M": M}.get(name, name)
        if mode == "cached" and find_chr(None, FF01, name) is None:
            # a missing handles.json is a harness fault, not a cached connect failure of the build
            d.rec(test=mode, name=name, result="no_handles", hint="run disc free first")
            sys.exit(1)
        await d.start([name])
        for r in range(rounds):
            check_api()
            if mode == "retry":
                att = []
                for _ in range(5):
                    res = await d.disc_once(name, 10, True, False)
                    att.append(res)
                    if "t_cmd" in res:
                        break
                    await asyncio.sleep(1)
                check_api()  # an API drop during the round is not the build's result
                # a harness fault (clear_failed, no_ff01) is not the build's
                if h := next((a["harness"] for a in att if a.get("harness")), None):
                    d.rec(test="retry", round=r, name=name, result=h)
                    continue
                d.rec(
                    test="retry",
                    round=r,
                    name=name,
                    attempts=len(att),
                    ok="t_cmd" in att[-1],
                    first_ok="t_cmd" in att[0],
                    detail=att,
                )
            else:
                res = await d.disc_once(
                    name, 60 if mode == "free" else 30, mode == "free", True
                )
                if h := res.get("harness"):
                    d.rec(test=mode, round=r, name=name, result=h)
                    continue
                check_api()  # an API drop during the round is not the build's result
                d.rec(test=mode, round=r, name=name, ok="t_cmd" in res, **res)
            await asyncio.sleep(2)
    elif cmd == "third":
        rounds, variant = int(sys.argv[4]), sys.argv[5]
        await d.start([L, M, EVE], 180)
        for r in range(rounds):
            check_api()
            held, third = {"eve": ([L, M], EVE), "L": ([M], L)}.get(variant, ([L], M))
            eve_held = variant != "eve"  # pd and L hold a fresh Eve link as well
            ok = True
            for h in held:
                ok = ok and await d.hold(h)
            t_eve = None
            if eve_held:
                # Eve drops about 10 s after connect, so a fresh cached link is the second held one
                if d.held(EVE):
                    await d.disconnect(EVE)
                    await asyncio.sleep(1)
                elif (old := d.st.pop(d.peers[EVE][0], None)) and old.get("cancel"):
                    # the dropped link's handler must not see the next connect, as in hold()
                    with contextlib.suppress(Exception):
                        old["cancel"]()
                st = await d.connect(EVE, True)
                ok = ok and st["connected"]
                t_eve = st.get("t_conn")
            if not ok:
                d.rec(
                    test="third",
                    variant=variant,
                    round=r,
                    result="held_failed",
                    t_eve=t_eve,
                )
                await d.drop_all()
                await asyncio.sleep(5)
                continue
            await asyncio.sleep(0.5)
            if not await d.clear(third):
                d.rec(test="third", variant=variant, round=r, result="clear_failed")
                continue
            st = await d.connect(third, False, timeout=30)
            res = {k: st.get(k) for k in ("connected", "t_conn", "err", "exc")}
            if st["connected"]:
                try:
                    t = time.monotonic()
                    await asyncio.wait_for(
                        cli.bluetooth_gatt_get_services(d.peers[third][0]), 30
                    )
                    res["t_svc_after"] = round(time.monotonic() - t, 3)
                except Exception as e:  # noqa: BLE001
                    res["svc_exc"] = f"{type(e).__name__}: {e}"
            # after the discovery too: a held link that dropped during it leaves only two links up
            held_ok = [d.held(h) for h in held] + ([d.held(EVE)] if eve_held else [])
            check_api()  # an API drop during the round is not the build's result
            d.rec(
                test="third",
                variant=variant,
                round=r,
                third=third,
                t_eve=t_eve,
                held_ok=held_ok,
                **res,
            )
            await d.disconnect(third)
            if eve_held:
                await d.disconnect(EVE)
            await asyncio.sleep(3)
        await d.drop_all()
    elif cmd == "notify":
        secs = float(sys.argv[4])
        await d.start([L])
        t_end = time.time() + secs
        stats = {
            "rx": 0,
            "gaps": 0,
            "missing": 0,
            "dups": 0,
            "drops": [],
            "reconnects": 0,
            "missing_drop": 0,
            "hold_failed": 0,
            "unheld_s": 0.0,
            "backwards": 0,
            "short": 0,
        }
        tick = find_chr(None, FF03, L)
        if tick is None or tick.cccd is None:
            d.rec(test="notify", result="no_handles", hint="run disc L first")
            sys.exit(1)
        last = [None]
        resumed = [False]

        def on_n(h, data) -> None:
            if len(data) < 4:  # not a tick; decoding it would read as a restart
                stats["short"] += 1
                return
            v = int.from_bytes(bytes(data[:4]), "little")
            stats["rx"] += 1
            if last[0] is not None and resumed[0]:
                # ticks sent while the link was down; lower means the server restarted meanwhile
                if v < last[0]:
                    stats["backwards"] += 1
                stats["missing_drop"] += max(0, v - last[0] - 1)
            elif last[0] is not None:
                if v == last[0]:
                    stats["dups"] += 1
                elif v > last[0] + 1:
                    stats["gaps"] += 1
                    stats["missing"] += v - last[0] - 1
                elif v < last[0]:  # the server restarted or the order broke
                    stats["backwards"] += 1
            last[0] = v
            resumed[0] = False

        # time without a subscribed link (start, drops, retries, failed holds) shows as missing ticks; count it
        # so it is not taken for loss, as selftest.hpp's tk_down_since
        down_since = time.time()
        while time.time() < t_end:
            check_api()
            if not await d.hold(L):
                stats["hold_failed"] += 1
                await asyncio.sleep(5)
                continue
            addr = d.peers[L][0]
            st = d.st[addr]
            rm = None  # only a subscription made in this pass is removed on failure
            try:
                # the tick holds 4 bytes; anything else is a stored handle from an older server table
                if (
                    len(await cli.bluetooth_gatt_read(addr, tick.handle, timeout=10))
                    != 4
                ):
                    d.rec(
                        test="notify", result="stale_handles", hint="run disc L first"
                    )
                    sys.exit(1)
                stop, rm = await cli.bluetooth_gatt_start_notify(
                    addr, tick.handle, on_n
                )
                await cli.bluetooth_gatt_write_descriptor(addr, tick.cccd, b"\x01\x00")
            except Exception as e:  # noqa: BLE001
                print(f"notify subscribe failed: {e}", flush=True)
                if rm:  # a handler left behind would count every tick twice after the next hold
                    rm()
                if st[
                    "lost"
                ]:  # the link dropped before the subscription: a drop, then hold again
                    stats["drops"].append([type(e).__name__, round(time.time(), 1)])
                    stats["reconnects"] += 1
                    d.st.pop(addr, None)
                    st["cancel"]()
                else:  # the link is up but a read or write failed: free the slot before holding again
                    stats.setdefault("subscribe_errors", []).append(
                        [f"{type(e).__name__}: {e}", round(time.time(), 1)]
                    )
                    await d.disconnect(L)
                await asyncio.sleep(2)
                continue
            stats["unheld_s"] = round(stats["unheld_s"] + time.time() - down_since, 1)
            down_since = None
            resumed[0] = True
            t_min = time.time()
            rx_min = stats["rx"]
            while time.time() < t_end and not st["lost"]:
                await asyncio.sleep(1)
                check_api()
                if time.time() - t_min >= 60:
                    d.rec(
                        test="notify_min",
                        rx=stats["rx"] - rx_min,
                        gaps=stats["gaps"],
                        missing=stats["missing"],
                        free=d.free[:2] if d.free else None,
                    )
                    t_min, rx_min = time.time(), stats["rx"]
            if st["lost"]:
                down_since = time.time()
                stats["drops"].append([st["lost"][0][0], round(time.time(), 1)])
                stats["reconnects"] += 1
                rm()  # the next hold registers a fresh handler
                d.st.pop(addr, None)
                st["cancel"]()
                await asyncio.sleep(2)
            else:
                await stop()
        if down_since is not None:  # the soak ended without the link
            stats["unheld_s"] = round(
                stats["unheld_s"] + max(0.0, min(time.time(), t_end) - down_since), 1
            )
        await d.disconnect(L)
        d.rec(test="notify", secs=secs, expected=round(secs * 5), **stats)
        # ticks seen or known missing should cover the held time; far less means the subscription went quiet
        want = secs * 5 - stats["unheld_s"] * 5
        # missing_drop is ticks sent while unheld, already left out of want
        covered = stats["rx"] + stats["missing"]
        if stats["rx"] and covered < 0.9 * want:
            d.rec(
                test="notify", result="notify_short", covered=covered, want=round(want)
            )
            sys.exit(1)
        # the expected count leaves out unheld time: a soak mostly without the link measured little
        if stats["unheld_s"] > 0.1 * secs:
            d.rec(
                test="notify",
                result="mostly_unheld",
                unheld_s=stats["unheld_s"],
                secs=secs,
            )
            sys.exit(1)
        if (
            stats["rx"] == 0
        ):  # L was never held or sent nothing: the step measured nothing
            sys.exit(1)
    elif cmd == "churn":
        cycles = int(sys.argv[4])
        await d.start([L, M])
        await asyncio.sleep(2)
        if (
            d.free is None
        ):  # no connections-free state: every cycle would read as a stuck slot
            d.rec(test="churn", result="no_free_state")
            sys.exit(1)
        limit = d.free[1]
        stuck = errs = not_seen = 0
        for c in range(cycles):
            check_api()
            name = (L, M)[c % 2]
            st = await d.connect(name, True, timeout=20)
            ok = st["connected"]
            # the slot must show as taken first, or the old free count reads as freed at once
            taken = False
            t = time.monotonic()
            while ok and time.monotonic() - t < 5:
                if taken := bool(d.free and d.free[0] < d.free[1]):
                    break
                await asyncio.sleep(0.1)
            not_seen += ok and not taken
            await asyncio.sleep(0.3)
            await d.disconnect(name)
            t = time.monotonic()
            while time.monotonic() - t < 10 and not (d.free and d.free[0] == d.free[1]):
                await asyncio.sleep(0.1)
            freed = bool(d.free and d.free[0] == d.free[1])
            stuck += not freed
            errs += not ok
            d.rec(
                test="churn_cycle",
                cycle=c,
                name=name,
                connected=ok,
                t_conn=st.get("t_conn"),
                err=st.get("err"),
                exc=st.get("exc"),
                slot_seen=taken,
                freed=freed,
                t_free=round(time.monotonic() - t, 3),
                free=d.free,
            )
            await asyncio.sleep(0.5)
        await asyncio.sleep(3)
        d.rec(
            test="churn",
            cycles=cycles,
            failed=errs,
            stuck=stuck,
            slot_not_seen=not_seen,
            limit=limit,
            free_end=d.free,
        )
        if errs == cycles:  # no cycle connected: nothing was measured
            sys.exit(1)
    elif cmd == "eve":
        rounds = int(sys.argv[4])
        await d.start([EVE], 180)
        for r in range(rounds):
            check_api()
            if not await d.clear(EVE):
                d.rec(test="eve", round=r, result="clear_failed")
                continue
            st = await d.connect(EVE, False, timeout=40)
            res = {k: st.get(k) for k in ("connected", "t_conn", "err", "exc")}
            addr = d.peers[EVE][0]
            if st["connected"]:
                try:
                    svcs = await asyncio.wait_for(
                        cli.bluetooth_gatt_get_services(addr), 30
                    )
                    res["t_svc"] = round(time.monotonic() - st["t0"], 3)
                    res["n"] = [
                        len(svcs.services),
                        sum(len(s.characteristics) for s in svcs.services),
                    ]
                    ch = next(
                        (
                            c
                            for s in svcs.services
                            for c in s.characteristics
                            if c.properties & 0x02
                        ),
                        None,
                    )
                    if (
                        ch is None
                    ):  # an empty or partial table: a harness fault, not the build's
                        res["result"] = "no_readable_char"
                    else:
                        await asyncio.wait_for(
                            cli.bluetooth_gatt_read(addr, ch.handle), 10
                        )
                        res["t_cmd"] = round(time.monotonic() - st["t0"], 3)
                except Exception as e:  # noqa: BLE001
                    res["exc"] = f"{type(e).__name__}: {e}"
                    res["t_fail"] = round(time.monotonic() - st["t0"], 3)
                res["lost"] = [(e, round(t - st["t0"], 3)) for e, t in st["lost"]]
            check_api()  # an API drop during the round is not the build's result
            d.rec(test="eve", round=r, ok="t_cmd" in res, **res)
            await d.disconnect(EVE)
            await asyncio.sleep(5)
    with contextlib.suppress(Exception):
        await asyncio.wait_for(cli.disconnect(), 5)


if __name__ == "__main__":
    asyncio.run(main())
