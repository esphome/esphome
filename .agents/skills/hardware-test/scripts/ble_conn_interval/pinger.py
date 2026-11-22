"""pinger.py HOST SECONDS OUT: one ICMP echo every 0.5 s, writes 'epoch rtt_ms|lost' lines."""

import pathlib
import subprocess
import sys
import time

host, secs, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
end = time.time() + secs
with pathlib.Path(out).open("a", encoding="utf-8") as f:
    while time.time() < end:
        t = time.time()
        r = subprocess.run(
            ["ping", "-c", "1", "-W", "2000", host],
            capture_output=True,
            text=True,
            check=False,
        )
        rtt = next(
            (
                ln.split("time=")[1].split()[0]
                for ln in r.stdout.splitlines()
                if "time=" in ln
            ),
            "lost",
        )
        f.write(f"{t:.3f} {rtt}\n")
        f.flush()
        time.sleep(max(0, 0.5 - (time.time() - t)))
