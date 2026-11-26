"""Pair kick.py's left-alone connections with the device's RESULT lines and judge them.

usage: judge.py KICK_TXT SERIAL_TXT CYCLES

A left-alone connection seen at host time T must have exactly one RESULT line stamped T+1 to T+12 s (it logs 6 s after
the connect, the next connect comes 8 s later), with got=25/25 and the CONN number kick.py expected. Both files are
stamped by this host's clock. Exits 1 on a missing or short RESULT, a CONN mismatch, or fewer than CYCLES connections.
"""

import pathlib
import re
import sys

ALONE = re.compile(r"^(\d\d:\d\d:\d\d) connection \d+ left alone \(device CONN (\d+)\)")
CONN = re.compile(r"^\[(\d\d:\d\d:\d\d)\].*: CONN (\d+)\s*$")
RESULT = re.compile(r"^\[(\d\d:\d\d:\d\d)\].*: (RESULT got=(\d+)/25 .*?)\s*$")


def secs(t: str) -> int:
    h, m, s = map(int, t.split(":"))
    return h * 3600 + m * 60 + s


kick_txt, serial_txt, cycles = sys.argv[1], sys.argv[2], int(sys.argv[3])
with pathlib.Path(kick_txt).open(encoding="utf-8", errors="replace") as f:
    alone = [(secs(m[1]), int(m[2])) for line in f if (m := ALONE.match(line))]
results = []  # (time, last CONN logged before it, got, text)
conn = None
with pathlib.Path(serial_txt).open(encoding="utf-8", errors="replace") as f:
    for line in f:
        if m := CONN.match(line):
            conn = int(m[2])
        elif m := RESULT.match(line):
            results.append((secs(m[1]), conn, int(m[3]), m[2]))

fail = 0
for t, want in alone:
    hits = [r for r in results if 1 <= (r[0] - t) % 86400 <= 12]
    if len(hits) != 1:
        print(f"CONN {want}: {len(hits)} RESULT lines in the window")
        fail += 1
        continue
    _, got_conn, got, text = hits[0]
    bad = got != 25 or got_conn != want
    fail += bad
    print(f"CONN {want}: device CONN {got_conn} {text}{'  <- bad' if bad else ''}")
print(f"left-alone connections judged: {len(alone)}, bad: {fail}")
if len(alone) < cycles:
    sys.exit(f"FAIL: only {len(alone)} left-alone connections for {cycles} cycles")
if fail:
    sys.exit(
        "FAIL: a left-alone connection missed messages or did not match its CONN number"
    )
print("PASS")
