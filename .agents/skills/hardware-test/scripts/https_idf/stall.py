"""Hold idle TCP connections on the HTTPS port (no ClientHello), switch the listener and time it.

usage: stall.py HOST   Exits 1 when a switch did not take effect while the idle clients were open.
"""

import socket
import ssl
import sys
import time
import urllib.request

HOST = sys.argv[1]
CTX = ssl.create_default_context()
CTX.check_hostname = False
CTX.verify_mode = ssl.CERT_NONE


def get(url: str) -> tuple[bool, str]:
    t = time.monotonic()
    try:
        with urllib.request.urlopen(url, context=CTX, timeout=10) as r:
            return r.status == 200, f"{r.status} {time.monotonic() - t:.2f}s"
    except Exception as e:  # noqa: BLE001
        return False, f"fail {type(e).__name__} {time.monotonic() - t:.2f}s"


ok_all = True
for target, url, back in (
    ("http", f"http://{HOST}/hw", f"http://{HOST}/hw/switch/https"),
    ("https8443", f"https://{HOST}:8443/hw", f"https://{HOST}:8443/hw/switch/https"),
):
    idle = [socket.create_connection((HOST, 443), timeout=5) for _ in range(2)]
    time.sleep(1)
    print(
        f"2 idle TCP clients open on 443; switch to {target}: {get(f'https://{HOST}/hw/switch/{target}')[1]}"
    )
    time.sleep(3)
    ok, text = get(url)
    ok_all &= ok
    print(f"  after the switch {url}: {text}")
    for s in idle:
        s.settimeout(2)
        try:
            print(
                "  idle client:", "closed by server" if s.recv(1) == b"" else "got data"
            )
        except OSError as e:
            print("  idle client:", type(e).__name__)
        s.close()
    print(f"  switch back: {get(back)[1]}")
    time.sleep(3)
    ok, text = get(f"https://{HOST}/hw")
    ok_all &= ok
    print(f"  https 443: {text}")
sys.exit(0 if ok_all else 1)
