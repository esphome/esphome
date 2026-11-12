"""Wi-Fi load on an ESP8266 under test: a fixed rate UDP flood to port 9 plus back to back web_server GETs.

usage: wifi_load.py HOST SECONDS PPS
Prints the counts every minute and a final "done" line.
"""

import http.client
import socket
import sys
import threading
import time
import urllib.request

host, secs, pps = sys.argv[1], float(sys.argv[2]), int(sys.argv[3])
end = time.monotonic() + secs
stats = {"udp": 0, "udp_err": 0, "http_ok": 0, "http_err": 0}


def http_load() -> None:
    while time.monotonic() < end:
        try:
            with urllib.request.urlopen(f"http://{host}/", timeout=5) as r:
                r.read()
            stats["http_ok"] += 1
        except (OSError, http.client.HTTPException):
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
print("done", stats, flush=True)
if stats["http_ok"] == 0 or stats["http_err"] > stats["http_ok"]:
    sys.exit("the device did not take the HTTP load; the run is invalid")
