"""Wi-Fi load on an ESP8266 under test: a fixed rate UDP flood to port 9 plus back to back web_server GETs.

usage: wifi_load.py HOST SECONDS PPS
Prints the counts every minute and a final "done" line.
"""

import socket
import sys
import threading
import time
import urllib.request

host, secs, pps = sys.argv[1], float(sys.argv[2]), int(sys.argv[3])
end = time.monotonic() + secs
stats = {"udp": 0, "udp_err": 0, "http_ok": 0, "http_err": 0, "http_gap_s": 0}


last_ok = [time.monotonic()]


def http_load() -> None:
    while time.monotonic() < end:
        try:
            with urllib.request.urlopen(f"http://{host}/", timeout=5) as r:
                r.read()
            stats["http_ok"] += 1
            now = time.monotonic()
            stats["http_gap_s"] = max(stats["http_gap_s"], round(now - last_ok[0]))
            last_ok[0] = now
        except Exception:  # noqa: BLE001  any failure counts; the thread must not die
            stats["http_err"] += 1
            time.sleep(1)


threading.Thread(target=http_load, daemon=True).start()
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
payload = b"x" * 512
nxt = last = time.monotonic()
while (now := time.monotonic()) < end:
    try:
        s.sendto(payload, (host, 9))
        stats["udp"] += 1
    except OSError:  # mostly ENOBUFS when the host outruns the network
        stats["udp_err"] += 1
        time.sleep(0.01)
    nxt += 1 / pps
    if (d := nxt - time.monotonic()) > 0:
        time.sleep(d)
    if now - last > 60:
        last = now
        print(time.strftime("%H:%M:%S"), stats, flush=True)
stats["http_gap_s"] = max(stats["http_gap_s"], round(time.monotonic() - last_ok[0]))
print("done", stats, flush=True)
if (
    stats["http_ok"] == 0
    or stats["http_gap_s"] > 60  # the device stopped answering for over a minute
    or stats["http_err"] > stats["http_ok"]
    or stats["udp"] < secs * pps / 2
):
    sys.exit("the load did not reach the device; the run is invalid")
