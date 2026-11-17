"""REST helpers for the async_bump scripts: set HOST, then req(), jget(), jv(), uptime(), heap().

usage: rest.py uptime|heap HOST   prints the whole seconds of uptime or the free heap; exits 1 with no answer.
"""

import json
import sys
import time
import urllib.error
import urllib.request

HOST = ""


def req(
    path: str, method: str = "GET", timeout: float = 10
) -> tuple[int, bytes, float]:
    """(HTTP status or -1, body or the error name, seconds taken)."""
    t = time.monotonic()
    r = urllib.request.Request(
        f"http://{HOST}{path}", method=method, data=b"" if method == "POST" else None
    )
    try:
        with urllib.request.urlopen(r, timeout=timeout) as resp:
            return resp.status, resp.read(), time.monotonic() - t
    except urllib.error.HTTPError as e:
        return e.code, e.read(), time.monotonic() - t
    except Exception as e:  # noqa: BLE001
        return -1, type(e).__name__.encode(), time.monotonic() - t


def jget(path: str, timeout: float = 10) -> dict:
    code, body, _ = req(path, timeout=timeout)
    try:
        return json.loads(body) if code == 200 else {"_code": code}
    except ValueError:
        return {"_code": code, "_bad": body[:80]}


def jv(path: str, key: str = "value", timeout: float = 10) -> object:
    return jget(path, timeout).get(key)


def uptime() -> object:
    return jv("/sensor/Uptime")


def heap() -> object:
    return jv("/sensor/Heap%20free")


if __name__ == "__main__":
    HOST = sys.argv[2]
    v = {"uptime": uptime, "heap": heap}[sys.argv[1]]()
    if v is None:
        sys.exit(1)
    print(int(v))
