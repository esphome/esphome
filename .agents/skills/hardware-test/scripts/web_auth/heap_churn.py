"""Heap under connection churn: free heap sampled while N wrong-hash requests run at a fixed rate and after.

usage: heap_churn.py HOST USER PASS [N] [RATE] [SAMPLE_S] [AFTER_S]
Prints one line per heap sample (seconds since start, phase, heap, uptime) and a summary: idle heap, lowest heap
during the load, heap at the end of the load, and seconds after the load until the heap is back within 2 KB of idle.
Built for lwIP TIME_WAIT checks (TCP_MSL): each closed connection may keep a PCB on the heap for 2 * TCP_MSL.
Exits 1 on a reboot, no idle heap reading, no recovery in the window, or when fewer than half the requests got a 401
that the device closed within 0.5 s ("401/noclose" does not load TIME_WAIT on the device).
"""

import pathlib
import sys
import threading
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import authfuzz as af  # noqa: E402

HOST, U, P = sys.argv[1:4]
N = int(sys.argv[4]) if len(sys.argv) > 4 else 600
RATE = float(sys.argv[5]) if len(sys.argv) > 5 else 3.0
SAMPLE = float(sys.argv[6]) if len(sys.argv) > 6 else 10.0
AFTER = float(sys.argv[7]) if len(sys.argv) > 7 else 150.0
af.configure(HOST, U, P)
read_fails = [0]


def val(name: str) -> float | None:
    if (v := af.sensor(name)) is None:
        read_fails[0] += 1
        return None
    return float(v)


t0 = time.monotonic()
idle = [h for h in (val("Heap%20free") for _ in range(3)) if h is not None]
idle_heap = max(idle) if idle else None
u_start = val("Uptime")
print(f"idle heap {idle} uptime {u_start}", flush=True)
codes: dict = {}
load_done = threading.Event()
load_done_at: list[float] = []
load_secs: list[float] = []


def load() -> None:
    nxt = load_start = time.monotonic()
    try:
        for _ in range(N):
            if (w := nxt - time.monotonic()) > 0:
                time.sleep(w)
            nxt += 1.0 / RATE
            k = af.bad_once()
            codes[k] = codes.get(k, 0) + 1
    finally:
        load_done_at.append(time.monotonic())
        # a board slower than RATE stretches the load; the summary reports the rate reached
        load_secs.append(load_done_at[0] - load_start)
        load_done.set()


th = threading.Thread(target=load)
th.start()
samples = []
load_end = None
recovered_at = None
while True:
    # 1 s samples for the first 20 s after the load, to time a fast recovery
    if load_end is None:
        load_done.wait(SAMPLE)  # wakes early when the load ends
    else:
        time.sleep(1 if time.monotonic() - t0 - load_end < 20 else SAMPLE)
    now = time.monotonic() - t0
    phase = "load" if not load_done.is_set() else "after"
    if phase == "after" and load_end is None:
        load_end = (load_done_at[0] - t0) if load_done_at else now
    h, u = val("Heap%20free"), val("Uptime")
    samples.append((now, phase, h, u))
    print(f"{now:6.0f}s {phase:5} heap {h} uptime {u}", flush=True)
    if (
        phase == "after"
        and recovered_at is None
        and h is not None
        and idle_heap
        and h >= idle_heap - 2048
    ):
        recovered_at = now - load_end
    if load_end is not None and now - load_end >= AFTER:
        break
th.join()
load_heaps = [h for _, ph, h, _ in samples if ph == "load" and h is not None]
ups = [u for *_, u in samples if u is not None]
# any drop between successive readings, so a reboot that ends above the first reading still counts
chain = ([u_start] if u_start is not None else []) + ups
reboot = any(b < a for a, b in zip(chain, chain[1:], strict=False))
print(
    f"SUMMARY requests {N} at {RATE}/s (reached {N / load_secs[0]:.1f}/s): {codes}; idle {idle_heap}, lowest during load {min(load_heaps) if load_heaps else None}, "
    f"last during load {load_heaps[-1] if load_heaps else None}, back within 2 KB of idle "
    f"{f'{recovered_at:.0f} s after the load (sampling every {SAMPLE:.0f} s)' if recovered_at is not None else 'never in the window'}"
    f"; reboot {'YES' if reboot else 'no'}; failed heap/uptime reads {read_fails[0]}",
    flush=True,
)
fails = []
if reboot:
    fails.append("reboot")
if idle_heap is None:
    fails.append("no idle heap reading")
if u_start is None or not ups:
    fails.append("no uptime reading, so a reboot cannot be ruled out")
if not load_heaps:
    fails.append("no heap reading during the load")
if recovered_at is None:
    fails.append("heap not back within 2 KB of idle")
if codes.get("401", 0) * 2 < N:
    fails.append(
        f"only {codes.get('401', 0)} of {N} requests got a 401 and a close from the device"
    )
print("PASS" if not fails else "FAIL: " + ", ".join(fails), flush=True)
sys.exit(1 if fails else 0)
