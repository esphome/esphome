"""Send one hostile request at a time and check uptime afterwards, to tie a reboot to a request.

usage: isolate.py HOST USER PASS [ROUNDS] [CASE,...]   (one line per case: uptime before/after, rebooted yes/no)
Exits 1 when any request was followed by a reboot or a device that stopped answering.
Reuses authfuzz.py's raw socket sender and request builder.
"""

import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import authfuzz as af  # noqa: E402

# the Uptime sensor updates every second; allow that plus request latency before calling a boot time later
BOOT_SLACK_S = 3.0


def boot_time() -> tuple[float | None, float | None]:
    """(uptime, estimated boot time on the local monotonic clock), retried for about 50 s so a rebooting device
    can come back; (None, None) when it never answers."""
    for _ in range(25):
        if (u := af.uptime()) is not None:
            return float(u), time.monotonic() - float(u)
        time.sleep(2)
    return None, None


def main() -> None:
    host, user, password = sys.argv[1:4]
    rounds = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    af.configure(host, user, password)
    good = af.GOOD.encode()

    def get(extra: bytes = b"", auth: bytes | None = None) -> bytes:
        return af.build("GET", "/", auth, extra=extra)

    cases = {
        "nul_in_auth": get(auth=good + b"\x00garbage"),
        "nul_in_other_header": get(b"X-A: a\x00b\r\n"),
        "nul_in_request_line": b"GET /\x00 HTTP/1.1\r\nHost: x\r\n\r\n",
        "auth_8k": get(auth=b"Basic " + b"A" * 8192),
        "garbage_32k": get(b"X-Garbage: " + b"Z" * 32768 + b"\r\n"),
        "nonl_64k": b"GET / HTTP/1.1\r\nHost: x\r\nX-G: " + b"Q" * 65536,
        "headers_200": get(
            b"".join(f"X-H{i}: {'v' * 60}\r\n".encode() for i in range(200))
        ),
        "control": get(auth=good),
    }
    only = sys.argv[5].split(",") if len(sys.argv) > 5 else list(cases)
    u0, b0 = boot_time()
    reboots = 0
    for r in range(rounds):
        for name in only:
            t = time.strftime("%H:%M:%S")
            res = af.raw(cases[name], timeout=6, read_body=False)
            head = res["head"].split("\r\n", 1)[0] or res["err"] or "closed"
            time.sleep(4)
            u1, b1 = boot_time()
            # a later boot time catches a reboot even when the uptime climbed past the previous reading
            rb = (
                "REBOOT"
                if (b0 is None or b1 is None or b1 - b0 > BOOT_SLACK_S)
                else "ok"
            )
            reboots += rb == "REBOOT"
            print(
                f"[{t}] round {r} {name:22} -> {head[:40]:40} uptime {u0} -> {u1} {rb}",
                flush=True,
            )
            u0, b0 = u1, b1
    print(
        "PASS" if not reboots else f"FAIL: {reboots} requests followed by a reboot",
        flush=True,
    )
    sys.exit(1 if reboots else 0)


if __name__ == "__main__":
    main()
