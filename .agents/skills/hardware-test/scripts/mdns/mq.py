"""Legacy unicast mDNS queries straight to a device, for networks where multicast from the host does not arrive.

usage: mq.py IP HOSTNAME dump              ask each question once and print the answers (exit 1 if one is missing)
       mq.py IP HOSTNAME hammer N [TIMEOUT]  N questions back to back, count misses per question
"""

import socket
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
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("", 0))
TIMEOUT = float(sys.argv[5]) if len(sys.argv) > 5 else 0.5
missing = 0
sock.settimeout(TIMEOUT)
qid = 0


def ask(name: str, rtype: int) -> list | None:
    global qid  # noqa: PLW0603
    qid = (qid + 1) & 0xFFFF
    out = DNSOutgoing(const._FLAGS_QR_QUERY, multicast=False, id_=qid)
    out.add_question(DNSQuestion(name, rtype, const._CLASS_IN))
    for pkt in out.packets():
        sock.sendto(pkt, (ip, 5353))
    deadline = time.monotonic() + TIMEOUT
    while time.monotonic() < deadline:
        try:
            data, _ = sock.recvfrom(9000)
        except TimeoutError:
            return None
        msg = DNSIncoming(data)
        if msg.id == qid:
            return msg.answers()
    return None


if mode == "dump":
    for key, (name, rtype) in QUESTIONS.items():
        ans = ask(name, rtype)
        if ans is None:
            print(f"{key}: no answer")
            missing += 1
            continue
        for rec in ans:
            text = repr(rec)
            if rec.type == const._TYPE_TXT:
                text = f"TXT {rec.name} {rec.text!r}"
            print(f"{key}: {text}")
else:
    n = int(sys.argv[4])
    ok = miss = 0
    per = dict.fromkeys(QUESTIONS, 0)
    t0 = time.monotonic()
    worst = 0.0
    for i in range(n):
        key = list(QUESTIONS)[i % len(QUESTIONS)]
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
    missing = 0 if ok else 1
sys.exit(1 if missing else 0)
