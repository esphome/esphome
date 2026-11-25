"""#22859 driver (aioesphomeapi against a bluetooth_proxy).

drive.py HOST scan SECONDS                      names and address types of the PEERS (and the HK_NAME device)
drive.py HOST third MODE ROUNDS OUT             hold two PEERS, connect the third (rotating), MODE cache|nocache
drive.py HOST disc NAME ROUNDS OUT [HELD,HELD]  no-cache connect to services resolved, BUDGET s, retry up to 5
drive.py HOST eve MODE ROUNDS OUT [HELD,HELD]   HK_NAME device: connect (+discovery when nocache), then disconnect
env: API_KEY, PEERS (the three gen_peri.py peripheral names, comma separated), HK_NAME (local name prefix of a real
device, default "Eve Flare"), BUDGET (default 10), COLD=1 (clear the proxy cache before each attempt), APILOG=debug.
Writes JSON lines to OUT; device log lines to OUT.log.
"""

import asyncio
import json
import os
import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from aioesphomeapi import LogLevel  # noqa: E402
from apiconn import adv_name, connect, log_text, stop_event  # noqa: E402

PEERS = [p for p in os.environ.get("PEERS", "").split(",") if p]
HK_NAME = os.environ.get("HK_NAME", "Eve Flare")
WRITE_UUID = "5ec2ff01-0000-1000-8000-00805f9b34fb"
BUDGET = float(os.environ.get("BUDGET", "10"))
MAX_ATTEMPTS = 5


class Driver:
    def __init__(self, cli, out: str) -> None:
        self.cli = cli
        self.out = pathlib.Path(out).open("a", encoding="utf-8")  # noqa: SIM115
        self.log = pathlib.Path(out + ".log").open("a", encoding="utf-8")  # noqa: SIM115
        self.flags = 0
        self.peers: dict[str, tuple[int, int, int]] = {}  # name -> (addr, type, rssi)
        self.conns: dict[int, dict] = {}
        self.marks: list[str] = []
        self.allnames: set = set()
        self.adv_count = 0
        self.t0: float | None = None  # wall clock start of the current round

    def rec(self, **kw) -> None:
        kw["ts"] = round(time.time(), 3)
        if self.t0 is not None:
            kw["start"] = round(self.t0, 3)
        self.out.write(json.dumps(kw) + "\n")
        self.out.flush()
        print(json.dumps(kw), flush=True)

    def on_log(self, msg) -> None:
        t = log_text(msg)
        self.log.write(f"{time.time():.3f} {t}\n")
        self.log.flush()
        if (
            "Rejected" in t
            or "status=133" in t
            or "0x200d" in t
            or "reason=0x" in t
            or "failed" in t.lower()
        ):
            self.marks.append(t)

    async def start(self, scan_s: float, want: list[str]) -> None:
        info = await self.cli.device_info()
        self.flags = info.bluetooth_proxy_feature_flags_compat(self.cli.api_version)
        self.cli.subscribe_logs(
            self.on_log,
            log_level=LogLevel.LOG_LEVEL_DEBUG
            if os.environ.get("APILOG") == "debug"
            else LogLevel.LOG_LEVEL_WARN,
        )
        self.scanner = None
        self.cli.subscribe_bluetooth_scanner_state(
            lambda m: setattr(self, "scanner", (m.state, m.mode))
        )

        def on_adv(m) -> None:
            self.adv_count += len(m.advertisements)
            for a in m.advertisements:
                n = adv_name(bytes(a.data))
                self.allnames.add(n)
                if n and (n in PEERS or n.startswith(HK_NAME)):
                    self.peers[n] = (a.address, a.address_type, a.rssi)

        # keep the advert subscription: without a subscriber the proxy drops links
        self.cli.subscribe_bluetooth_le_raw_advertisements(on_adv)
        end = time.monotonic() + scan_s
        while time.monotonic() < end and not all(
            any(p.startswith(w) for p in self.peers) for w in want
        ):
            await asyncio.sleep(0.5)

    async def connect(self, name: str, cache: bool, timeout: float = 20) -> dict:
        addr, atype, _ = self.peers[name]
        st = {"name": name, "lost": [], "first": None}
        ev = asyncio.Event()

        def on_state(c: bool, mtu: int, err: int) -> None:
            if st["first"] is None:
                st["first"] = (c, mtu, err, time.monotonic())
                ev.set()
            elif not c:
                st["lost"].append((err, time.monotonic()))

        self.marks = []
        t0 = time.monotonic()
        st["t0"] = t0
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
            st["t_conn"] = round(t1 - t0, 3)
        else:
            st["connected"] = False
        if st["connected"]:
            self.conns[addr] = st
        return st

    async def disconnect(self, name: str) -> None:
        addr = self.peers[name][0]
        st = self.conns.pop(addr, None)
        try:
            await asyncio.wait_for(self.cli.bluetooth_device_disconnect(addr), 15)
        except Exception as e:  # noqa: BLE001
            print(f"disconnect {name}: {e}", flush=True)
        if st and st.get("cancel"):
            st["cancel"]()

    def held(self, name: str) -> bool:
        if name not in self.peers:
            return False
        st = self.conns.get(self.peers[name][0])
        return bool(st) and not st["lost"]

    async def ensure_held(self, names: list[str], cache: bool) -> bool:
        for n in names:
            if self.held(n):
                continue
            self.conns.pop(self.peers[n][0], None)
            for _ in range(5):
                st = await self.connect(n, cache)
                if st["connected"]:
                    if not cache:
                        try:
                            await self.cli.bluetooth_gatt_get_services(self.peers[n][0])
                        except Exception as e:  # noqa: BLE001
                            print(f"held {n} services failed: {e}", flush=True)
                            await self.disconnect(n)
                            await asyncio.sleep(2)
                            continue
                    break
                print(
                    f"held {n} connect failed: {st.get('err')} {st.get('exc')}",
                    flush=True,
                )
                await asyncio.sleep(3)
            else:
                return False
        return True

    async def discover_once(self, name: str, write: bool, read: bool = False) -> dict:
        """One no-cache attempt: connect, services, optional first write, all inside BUDGET."""
        if os.environ.get("COLD") == "1":
            for _ in range(4):
                try:
                    r = await self.cli.bluetooth_device_clear_cache(self.peers[name][0])
                    if r.success:
                        break
                    print(f"clear cache failed: {r}", flush=True)
                except Exception as e:  # noqa: BLE001
                    print(f"clear cache error: {e}", flush=True)
                await asyncio.sleep(3)
            else:
                return {"connected": False, "exc": "clear cache failed"}
        adv0 = self.adv_count
        st = await self.connect(name, False, timeout=BUDGET)
        res = {
            "connected": st["connected"],
            "t_conn": st.get("t_conn"),
            "err": st.get("err"),
            "exc": st.get("exc"),
        }
        if not st["connected"]:
            res["marks"] = self.marks[-6:]
            return res
        addr = self.peers[name][0]
        remaining = BUDGET - (time.monotonic() - st["t0"])
        try:
            svcs = await asyncio.wait_for(
                self.cli.bluetooth_gatt_get_services(addr), max(remaining, 0.1)
            )
            res["t_svc"] = round(time.monotonic() - st["t0"], 3)
            res["n_svc"] = len(svcs.services)
            res["n_chr"] = sum(len(s.characteristics) for s in svcs.services)
            res["n_dsc"] = sum(
                len(c.descriptors) for s in svcs.services for c in s.characteristics
            )
            if write or read:
                ch = next(
                    c
                    for s in svcs.services
                    for c in s.characteristics
                    if (c.uuid == WRITE_UUID if write else c.properties & 0x02)
                )
                op = (
                    self.cli.bluetooth_gatt_write(addr, ch.handle, b"\x01", True)
                    if write
                    else self.cli.bluetooth_gatt_read(addr, ch.handle)
                )
                remaining = BUDGET - (time.monotonic() - st["t0"])
                await asyncio.wait_for(op, max(remaining, 0.1))
                res["t_cmd"] = round(time.monotonic() - st["t0"], 3)
        except TimeoutError:
            res["timeout"] = round(time.monotonic() - st["t0"], 3)
        except Exception as e:  # noqa: BLE001
            res["exc"] = f"{type(e).__name__}: {e}"
            res["t_fail"] = round(time.monotonic() - st["t0"], 3)
        res["lost"] = [(e, round(t - st["t0"], 3)) for e, t in st["lost"]]
        res["scanner"] = str(self.scanner)
        res["adv_per_s"] = round(
            (self.adv_count - adv0) / max(time.monotonic() - st["t0"], 0.1), 1
        )
        res["held_now"] = [n for n in PEERS if n != name and self.held(n)]
        res["marks"] = self.marks[-6:]
        await self.disconnect(name)
        return res


async def main() -> None:
    host, cmd = sys.argv[1], sys.argv[2]
    lost_api, on_stop = stop_event()
    cli = await connect(host, on_stop=on_stop)
    if cmd == "scan":
        d = Driver(cli, "scan.jsonl")
        await d.start(float(sys.argv[3]), [*PEERS, HK_NAME])
        for n, (_a, t, r) in d.peers.items():
            print(f"{n} type={t} rssi={r} seen")
        print(sorted(x for x in d.allnames if x and x not in PEERS))
        await cli.disconnect()
        return
    if cmd == "third" and len(PEERS) != 3:
        sys.exit("set PEERS to the three peripheral names")
    if cmd == "third":
        mode, rounds, out = sys.argv[3], int(sys.argv[4]), sys.argv[5]
        cache = mode == "cache"
        d = Driver(cli, out)
        await d.start(30, PEERS)
        missing = [b for b in PEERS if b not in d.peers]
        if missing:
            sys.exit(f"not seen: {missing}")
        for r in range(rounds):
            d.t0 = time.time()
            if lost_api.is_set():
                print("API connection lost, stopping", flush=True)
                sys.exit(3)
            third = PEERS[r % 3]
            held = [b for b in PEERS if b != third]
            if d.held(third):
                await d.disconnect(third)
                await asyncio.sleep(2)
            if not await d.ensure_held(held, cache):
                d.rec(round=r, mode=mode, third=third, result="held_failed")
                continue
            # give the proxy time to see a recent advert from the third
            await asyncio.sleep(3)
            st = await d.connect(third, cache)
            await asyncio.sleep(0.5)
            marks = list(d.marks)
            refused = any("Rejected" in m or "status=133" in m for m in marks)
            d.rec(
                round=r,
                mode=mode,
                third=third,
                connected=st["connected"],
                err=st.get("err"),
                t_conn=st.get("t_conn"),
                exc=st.get("exc"),
                refused_log=refused,
                marks=marks[-6:],
                held_ok=[d.held(h) for h in held],
            )
            if st["connected"] and not cache:
                try:
                    await asyncio.wait_for(
                        d.cli.bluetooth_gatt_get_services(d.peers[third][0]), 15
                    )
                except Exception as e:  # noqa: BLE001
                    print(f"third services: {e}", flush=True)
            await d.disconnect(third)
            await asyncio.sleep(3)
        for b in PEERS:
            if d.peers[b][0] in d.conns:
                await d.disconnect(b)
        await cli.disconnect()
        return
    if cmd in ("disc", "eve"):
        # disc takes a peer NAME where eve takes a MODE
        arg, rounds, out = sys.argv[3], int(sys.argv[4]), sys.argv[5]
        held = sys.argv[6].split(",") if len(sys.argv) > 6 else []
        name, mode = (arg, "nocache") if cmd == "disc" else (None, arg)
        d = Driver(cli, out)
        await d.start(
            180 if cmd == "eve" else 30, ([name] if name else [HK_NAME]) + held
        )
        if cmd == "eve":
            eves = [n for n in d.peers if n.startswith(HK_NAME)]
            if not eves:
                d.rec(result="eve_not_seen")
                await cli.disconnect()
                return
            name = min(eves)
            print(f"using {name} rssi={d.peers[name][2]}", flush=True)
        for r in range(rounds):
            d.t0 = time.time()
            if lost_api.is_set():
                print("API connection lost, stopping", flush=True)
                sys.exit(3)
            if held and not await d.ensure_held(held, True):
                d.rec(round=r, name=name, result="held_failed")
                continue
            if cmd == "disc":
                attempts = []
                for _a in range(MAX_ATTEMPTS):
                    res = await d.discover_once(name, write=True)
                    attempts.append(res)
                    if "t_cmd" in res:
                        break
                    await asyncio.sleep(1)
                ok = "t_cmd" in attempts[-1]
                d.rec(
                    round=r,
                    name=name,
                    held=held,
                    attempts=len(attempts),
                    ok=ok,
                    detail=attempts,
                )
                await asyncio.sleep(2)
            else:
                if mode == "cache":
                    st = await d.connect(name, True, timeout=20)
                    d.rec(
                        round=r,
                        name="eve",
                        mode=mode,
                        held=held,
                        connected=st["connected"],
                        t_conn=st.get("t_conn"),
                        err=st.get("err"),
                        exc=st.get("exc"),
                        marks=d.marks[-6:],
                    )
                    if st["connected"]:
                        await d.disconnect(name)
                else:
                    rssi = d.peers[name][2]
                    res = await d.discover_once(name, write=False, read=True)
                    d.rec(
                        round=r,
                        name="eve",
                        mode=mode,
                        held=held,
                        rssi_before=rssi,
                        **res,
                    )
                await asyncio.sleep(5)
        for b in held:
            await d.disconnect(b)
        await cli.disconnect()


asyncio.run(main())
