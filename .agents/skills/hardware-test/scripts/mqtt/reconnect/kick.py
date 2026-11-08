"""Kick every other connection of a device 0 to 400 ms after it connects, by taking over its client id.

usage: kick.py BROKER_LOG BROKER_PORT PREFIX CYCLES

Tails the mosquitto log (log_type all), so mosquitto drops the device's session with "session taken over". The
connection after each kick is left alone for 8 s (the device logs its RESULT 6 s after a connect), then ended to
start the next cycle. The first kick hits the connection that is already up (the device's CONN 1), so connection n
seen here is the device's CONN n + 1. The kicker's own connections come from 127.0.0.1 with the same id and are
skipped. Each left-alone connection prints "HH:MM:SS connection n left alone (device CONN n+1)" for run.sh to pair
with the device's RESULT lines.
"""

import pathlib
import random
import re
import subprocess
import sys
import time

LOG, PORT, PREFIX, CYCLES = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
PAT = re.compile(
    rf"New client connected from (?!127\.0\.0\.1)\S+ as ({re.escape(PREFIX)}-[0-9a-f]+)"
)


def kick(client_id: str) -> None:
    subprocess.run(
        [
            "mosquitto_pub",
            "-h",
            "127.0.0.1",
            "-p",
            PORT,
            "-i",
            client_id,
            "-t",
            "kick/x",
            "-m",
            "1",
        ],
        check=True,
    )


with pathlib.Path(LOG).open("rb") as log:
    # the device connected seconds ago, so its id is in the log's tail
    log.seek(max(0, log.seek(0, 2) - 256 * 1024))
    ids = PAT.findall(log.read().decode(errors="replace"))
    if not ids:
        sys.exit(f"no {PREFIX}- client in {LOG} yet")
    kick(ids[-1])
    n = 0
    while n < CYCLES * 2:
        line = log.readline()
        if not line:
            time.sleep(0.001)
            continue
        if not (m := PAT.search(line.decode(errors="replace"))):
            continue
        n += 1
        if n % 2 == 1:
            d = random.uniform(0, 0.4)
            time.sleep(d)
            kick(m.group(1))
            print(
                f"{time.strftime('%H:%M:%S')} connection {n} kicked after {d * 1000:.0f} ms",
                flush=True,
            )
        else:
            print(
                f"{time.strftime('%H:%M:%S')} connection {n} left alone (device CONN {n + 1})",
                flush=True,
            )
            time.sleep(8)
            log.seek(0, 2)  # skip the 8 s, but not the reconnect this kick causes
            kick(m.group(1))
