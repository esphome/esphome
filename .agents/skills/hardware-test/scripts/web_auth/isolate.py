"""Send one hostile request at a time and check uptime afterwards, to tie a reboot to a request.

usage: isolate.py HOST USER PASS [ROUNDS] [CASE,...]   (one line per case: uptime before/after, rebooted yes/no)
Reuses authfuzz.py's raw socket sender and request builder.
"""

import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import authfuzz as af  # noqa: E402


def uptime() -> float | None:
    """The device's uptime, retried for about 50 s so a rebooting device can come back."""
    for _ in range(25):
        if (u := af.uptime()) is not None:
            return float(u)
        time.sleep(2)
    return None


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
    u0 = uptime()
    for r in range(rounds):
        for name in only:
            t = time.strftime("%H:%M:%S")
            res = af.raw(cases[name], timeout=6, read_body=False)
            head = res["head"].split("\r\n", 1)[0] or res["err"] or "closed"
            time.sleep(4)
            u1 = uptime()
            rb = "REBOOT" if (u0 is None or u1 is None or u1 < u0) else "ok"
            print(
                f"[{t}] round {r} {name:22} -> {head[:40]:40} uptime {u0} -> {u1} {rb}",
                flush=True,
            )
            u0 = u1


if __name__ == "__main__":
    main()
