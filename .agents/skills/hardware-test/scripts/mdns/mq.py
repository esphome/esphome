"""Legacy unicast mDNS queries straight to a device, for networks where multicast from the host does not arrive.

usage: mq.py IP HOSTNAME dump              ask each question once and print the answers (exit 1 if one is missing)
       mq.py IP HOSTNAME hammer N [TIMEOUT]  N questions back to back, count misses per question (exit 1 when more
                                           than MAX_MISS_PCT percent were missed; default 100, so only when none
                                           was answered)
       mq.py IP HOSTNAME lat N             N A queries 300 ms apart; per-query latency, answers after 1.5 s are late
                                           (exit 1 when none was answered)
"""

import os
import select
import socket
import statistics
import sys
import time

from zeroconf import DNSIncoming, DNSOutgoing, DNSQuestion, const

ip, host, mode = sys.argv[1], sys.argv[2], sys.argv[3]
INST = f"{host}._hwtest._tcp.local."
QUESTIONS = {
    "ptr_esphomelib": ("_esphomelib._tcp.local.", const._TYPE_PTR),
    "ptr_hwtest": ("_hwtest._tcp.local.", const._TYPE_PTR),
    "a": (f"{host}.local.", const._TYPE_A),
    "srv": (INST, const._TYPE_SRV),
    "txt": (INST, const._TYPE_TXT),
    "srv_api": (f"{host}._esphomelib._tcp.local.", const._TYPE_SRV),
    "txt_api": (f"{host}._esphomelib._tcp.local.", const._TYPE_TXT),
}
KEYS = list(QUESTIONS)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("", 0))
TIMEOUT = float(sys.argv[5]) if len(sys.argv) > 5 else 0.5
missing = 0
sock.settimeout(TIMEOUT)
qid = 0


def send(name: str, rtype: int, query_id: int) -> None:
    out = DNSOutgoing(const._FLAGS_QR_QUERY, multicast=False, id_=query_id)
    out.add_question(DNSQuestion(name, rtype, const._CLASS_IN))
    for pkt in out.packets():
        sock.sendto(pkt, (ip, 5353))


def ask(name: str, rtype: int) -> list | None:
    """The answers for NAME and RTYPE only (empty when the reply has none of them), or None without a reply."""
    global qid  # noqa: PLW0603
    qid = (qid + 1) & 0xFFFF
    send(name, rtype, qid)
    deadline = time.monotonic() + TIMEOUT
    while time.monotonic() < deadline:
        try:
            data, _ = sock.recvfrom(9000)
        except TimeoutError:
            return None
        msg = DNSIncoming(data)
        if msg.id == qid:
            return [
                a
                for a in msg.answers()
                if a.type == rtype and a.name.lower() == name.lower()
            ]
    return None


def latency(n: int) -> None:
    sent: dict[int, float] = {}
    lat: dict[int, float] = {}

    def drain(until: float) -> None:
        while (left := until - time.monotonic()) > 0:
            if not select.select([sock], [], [], left)[0]:
                return
            msg = DNSIncoming(sock.recvfrom(9000)[0])
            if msg.id in sent and msg.id not in lat:
                lat[msg.id] = time.monotonic() - sent[msg.id]

    for query_id in range(1, n + 1):
        sent[query_id] = time.monotonic()
        send(*QUESTIONS["a"], query_id)
        drain(time.monotonic() + 0.3)
    drain(time.monotonic() + 2)
    if not lat:
        sys.exit(f"{n} sent, none answered")
    v = sorted(x * 1000 for x in lat.values())
    late = sum(x > 1500 for x in v)

    def q(p: float) -> float:
        return v[min(len(v) - 1, int(p * len(v)))]

    print(
        f"{n} sent, {len(v)} answered, {n - len(v)} never, {late} after 1.5 s; median {statistics.median(v):.0f} ms, "
        f"p90 {q(0.9):.0f} ms, p99 {q(0.99):.0f} ms, max {v[-1]:.0f} ms"
    )


if mode == "dump":
    for key, (name, rtype) in QUESTIONS.items():
        ans = ask(name, rtype)
        if not ans:
            print(f"{key}: {'no answer' if ans is None else 'no matching record'}")
            missing += 1
            continue
        for rec in ans:
            text = repr(rec)
            if rec.type == const._TYPE_TXT:
                text = f"TXT {rec.name} {rec.text!r}"
            print(f"{key}: {text}")
elif mode == "lat":
    latency(int(sys.argv[4]))
else:
    n = int(sys.argv[4])
    ok = miss = 0
    per = dict.fromkeys(QUESTIONS, 0)
    t0 = time.monotonic()
    worst = 0.0
    for i in range(n):
        key = KEYS[i % len(KEYS)]
        name, rtype = QUESTIONS[key]
        s = time.monotonic()
        ans = ask(name, rtype)
        dt = time.monotonic() - s
        if ans:
            ok += 1
            worst = max(worst, dt)
        else:
            miss += 1
            per[key] += 1
    print(
        f"hammer {n} queries in {time.monotonic() - t0:.1f}s: answered {ok}, missed {miss}, worst {worst * 1000:.0f} ms, misses by question {per}"
    )
    max_miss_pct = float(os.environ.get("MAX_MISS_PCT", "100"))
    missing = not ok or miss * 100 > max_miss_pct * n
    if missing:
        print(f"FAIL: missed {miss} of {n}, the limit is {max_miss_pct:g}%")
sys.exit(1 if missing else 0)
