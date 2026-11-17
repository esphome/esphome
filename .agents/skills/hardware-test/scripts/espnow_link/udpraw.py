"""udpraw.py SECONDS HOST [HOST...]: every second send a 16 byte datagram from this host to ports 21121, 21125 and
21126 of each HOST, and count datagrams arriving on port 21124 (the boards' several-address link). Exits 1 when none
arrived."""

import socket
import sys
import time

# Not the magic of any entest stream, so a link that accepts third-party datagrams counts these as foreign
MAGIC = b"\xa5\x5d"
secs, hosts = float(sys.argv[1]), sys.argv[2:]
tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
rx.bind(("0.0.0.0", 21124))
rx.settimeout(0.05)
end, nxt, sent, got, src = time.time() + secs, 0, 0, 0, {}
while time.time() < end:
    if time.time() >= nxt:
        for h in hosts:
            for p in (21121, 21125, 21126):
                tx.sendto(MAGIC + bytes(14), (h, p))
                sent += 1
        nxt = time.time() + 1
    try:
        d, a = rx.recvfrom(2048)
        got += 1
        src[a[0]] = src.get(a[0], 0) + 1
    except TimeoutError:
        pass
print(f"sent={sent} received_on_21124={got} by_source={src}")
sys.exit(0 if got else "FAIL nothing arrived on port 21124")
