"""A queries 300 ms apart as legacy unicast straight to the device; per-query latency, answers after 1.5 s are late.

usage: lat.py IP HOSTNAME N   Exits 1 when no query was answered.
"""

import socket
import statistics
import sys
import time

from zeroconf import DNSIncoming, DNSOutgoing, DNSQuestion, const

ip, host, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("", 0))
sock.setblocking(False)
sent: dict[int, float] = {}
lat: dict[int, float] = {}
late = 0


def drain() -> None:
    global late  # noqa: PLW0603
    while True:
        try:
            data, _ = sock.recvfrom(9000)
        except BlockingIOError:
            return
        msg = DNSIncoming(data)
        if msg.id in sent and msg.id not in lat:
            lat[msg.id] = time.monotonic() - sent[msg.id]
            if lat[msg.id] > 1.5:
                late += 1


for qid in range(1, n + 1):
    out = DNSOutgoing(const._FLAGS_QR_QUERY, multicast=False, id_=qid)
    out.add_question(DNSQuestion(f"{host}.local.", const._TYPE_A, const._CLASS_IN))
    sent[qid] = time.monotonic()
    sock.sendto(out.packets()[0], (ip, 5353))
    end = time.monotonic() + 0.3
    while time.monotonic() < end:
        drain()
        time.sleep(0.002)
time.sleep(2)
drain()
if not lat:
    sys.exit(f"{n} sent, none answered")
v = sorted(x * 1000 for x in lat.values())


def q(p: float) -> float:
    return v[min(len(v) - 1, int(p * len(v)))]


print(
    f"{n} sent, {len(v)} answered, {n - len(v)} never, {late} after 1.5 s; median {statistics.median(v):.0f} ms, "
    f"p90 {q(0.9):.0f} ms, p99 {q(0.99):.0f} ms, max {v[-1]:.0f} ms"
)
