"""Hold idle TCP connections on the HTTPS port (no ClientHello) while an armed listener switch runs.

usage: stall.py HOST   Exits 1 when a switch request was refused, a switch did not take effect while the idle clients
were open, or an idle client was not closed by the server.
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


def get(url: str, want: int = 200) -> tuple[bool, str]:
    t = time.monotonic()
    try:
        with urllib.request.urlopen(url, context=CTX, timeout=10) as r:
            return r.status == want, f"{r.status} {time.monotonic() - t:.2f}s"
    except Exception as e:  # noqa: BLE001
        return False, f"fail {type(e).__name__} {time.monotonic() - t:.2f}s"


ok_all = True
for target, url, back in (
    ("http", f"http://{HOST}/hw", f"http://{HOST}/hw/switch/https"),
    ("https8443", f"https://{HOST}:8443/hw", f"https://{HOST}:8443/hw/switch/https"),
):
    # an idle client holds the server task, so the switch is armed 3 s ahead on the main loop before they open
    ok, text = get(f"https://{HOST}/hw/switch/{target}/later", 202)
    ok_all &= ok
    print(f"switch to {target} armed: {text}")
    idle = [socket.create_connection((HOST, 443), timeout=5) for _ in range(2)]
    print("  2 idle TCP clients open on 443")
    time.sleep(5)
    ok, text = get(url)
    ok_all &= ok
    print(f"  after the switch {url}: {text}")
    for s in idle:
        s.settimeout(2)
        try:
            closed = s.recv(1) == b""
            print("  idle client:", "closed by server" if closed else "got data")
        except ConnectionResetError:
            closed = True
            print("  idle client: reset by server")
        except OSError as e:
            closed = False
            print("  idle client: still open,", type(e).__name__)
        ok_all &= closed
        s.close()
    ok, text = get(back, 202)
    ok_all &= ok
    print(f"  switch back: {text}")
    time.sleep(3)
    ok, text = get(f"https://{HOST}/hw")
    ok_all &= ok
    print(f"  https 443: {text}")
sys.exit(0 if ok_all else 1)
