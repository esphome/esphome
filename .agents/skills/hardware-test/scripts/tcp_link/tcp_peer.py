"""Raw peer for the tcp packet_interface link (esphome#21119): 2 byte little endian length, then the packet; a zero
length packet is a keep-alive. Logs every connection, frame and close with times, so keep-alive pacing, the receive
timeout, reconnects and oversized-length handling can be read from the output.

  tcp_peer.py client HOST PORT [opts]      connect (again after each close until --duration ends)
  tcp_peer.py server PORT [opts]           accept connections, one thread each

  --duration S        stop after S seconds (default 60)
  --keepalive S       send a keep-alive every S seconds (0 = never; default 2)
  --silent-after S    stop sending anything S seconds into each connection (keep reading)
  --oversize-after S N  S seconds into the first connection send a header announcing N bytes, then hold the socket
  --send SIZES        comma list of packet sizes sent once after connecting, payload byte i = (seq + i) & 0xff
  --bind ADDR         source address (client)
  --once              client: do not reconnect after a close
  --hold S            client: close after S seconds of connection (0 = keep)
  --expect-reject     client: pass only when the peer accepts the connection, then closes or resets it before
                      sending anything (how an allowed_ips server rejects a source); a refused connect fails
Exits 1 when no connection was made, a --send size did not go out, or the peer stayed connected after --oversize-after.
"""

import argparse
import contextlib
import socket
import struct
import sys
import threading
import time

T0 = time.monotonic()
LOCK = threading.Lock()


def log(*a):
    with LOCK:
        print(f"{time.monotonic() - T0:8.3f}", *a, flush=True)


def recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


# what the exit status checks: connections made, and whether the peer closed after an oversize header
RESULT = {"conns": 0, "over_closed": False, "rejected": False, "sent_all": False}


def run_conn(sock, tag, args, first):
    start = time.monotonic()
    stop = threading.Event()
    stats = {
        "frames": 0,
        "ka": [],
        "sizes": [],
        "last_ka": None,
        "over": False,
        "local_close": False,
    }
    RESULT["conns"] += 1

    def sender():
        last = 0.0
        sent_sizes = False
        over_done = False
        while not stop.is_set() and time.monotonic() - T0 < args.duration:
            el = time.monotonic() - start
            silent = args.silent_after is not None and el >= args.silent_after
            try:
                if (
                    first
                    and args.oversize_after
                    and not over_done
                    and el >= args.oversize_after[0]
                ):
                    sock.sendall(struct.pack("<H", int(args.oversize_after[1])))
                    log(tag, "SENT oversize header", int(args.oversize_after[1]))
                    over_done = stats["over"] = True
                if not silent and not sent_sizes and args.send:
                    for seq, size in enumerate(int(x) for x in args.send.split(",")):
                        payload = bytes([seq & 0xFF, seq >> 8]) + bytes(
                            (seq + i) & 0xFF for i in range(2, size)
                        )
                        sock.sendall(struct.pack("<H", size) + payload[:size])
                        log(tag, "SENT size", size, "sum", sum(payload[:size]))
                        time.sleep(0.2)
                    sent_sizes = RESULT["sent_all"] = True
                if (
                    not silent
                    and not over_done
                    and args.keepalive
                    and el - last >= args.keepalive
                ):
                    sock.sendall(b"\x00\x00")
                    last = el
                if args.hold and el >= args.hold:
                    log(tag, "HOLD over, closing")
                    stats["local_close"] = True
                    sock.shutdown(socket.SHUT_RDWR)
                    return
            except OSError as e:
                log(tag, "send error", e)
                return
            time.sleep(0.05)

    th = threading.Thread(target=sender, daemon=True)
    th.start()
    reason = "duration"
    remote = False  # the peer closed or reset the connection (not a timeout or our own shutdown)
    sock.settimeout(0.5)
    while time.monotonic() - T0 < args.duration:
        try:
            first_byte = recv_exact(sock, 1)
        except TimeoutError:
            continue  # nothing of the next header has arrived yet
        except OSError as e:
            reason = f"error {e}"
            remote = isinstance(e, (ConnectionResetError, BrokenPipeError))
            break
        if first_byte is None:
            reason = "EOF (peer closed)"
            remote = True
            break
        # the rest of the frame is read with a long timeout, so a slow header byte is not lost
        sock.settimeout(5)
        try:
            rest = recv_exact(sock, 1)
            if rest is None:
                reason = "EOF in header"
                remote = True
                break
            (n,) = struct.unpack("<H", first_byte + rest)
            body = recv_exact(sock, n) if n else b""
        except OSError as e:
            reason = f"error in frame {e}"
            remote = isinstance(e, (ConnectionResetError, BrokenPipeError))
            break
        sock.settimeout(0.5)
        if body is None:
            reason = "EOF in body"
            remote = True
            break
        stats["frames"] += 1
        now = time.monotonic()
        if n == 0:
            if stats["last_ka"] is not None:
                stats["ka"].append(now - stats["last_ka"])
            stats["last_ka"] = now
        else:
            stats["sizes"].append(n)
            stats["last_ka"] = (
                None  # a keep-alive is only due after an idle keep_alive interval
            )
            log(tag, "RX size", n)
    stop.set()
    dur = time.monotonic() - start
    ka = stats["ka"]
    kas = (
        f"keepalive gaps n={len(ka)} min={min(ka):.2f} max={max(ka):.2f}"
        if ka
        else "keepalive gaps n=0"
    )
    log(
        tag,
        f"CLOSE after {dur:.2f}s: {reason}; frames={stats['frames']} data={len(stats['sizes'])} {kas}",
    )
    remote = remote and not stats["local_close"]
    if stats["over"] and remote:
        RESULT["over_closed"] = True
    if remote and stats["frames"] == 0:
        RESULT["rejected"] = True  # closed by the peer before it sent anything
    with contextlib.suppress(OSError):
        sock.close()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("role", choices=["client", "server"])
    p.add_argument("a")
    p.add_argument("b", nargs="?")
    p.add_argument("--duration", type=float, default=60)
    p.add_argument("--keepalive", type=float, default=2)
    p.add_argument("--silent-after", type=float)
    p.add_argument("--oversize-after", nargs=2, type=float)
    p.add_argument("--send")
    p.add_argument("--bind")
    p.add_argument("--once", action="store_true")
    p.add_argument("--hold", type=float, default=0)
    p.add_argument("--expect-reject", action="store_true")
    args = p.parse_args()
    if args.role == "client":
        host, port = args.a, int(args.b)
        first = True
        while time.monotonic() - T0 < args.duration:
            s = socket.socket()
            if args.bind:
                s.bind((args.bind, 0))
            s.settimeout(5)
            try:
                s.connect((host, port))
            except OSError as e:
                log("client connect failed", e)
                s.close()
                if args.once:
                    break
                time.sleep(1)
                continue
            log("client CONNECT from", s.getsockname()[0])
            run_conn(s, "client", args, first)
            first = False
            if args.once:
                break
            time.sleep(0.5)
    else:
        port = int(args.a)
        ls = socket.socket()
        ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        ls.bind(("0.0.0.0", port))
        ls.listen(4)
        ls.settimeout(0.5)
        n = 0
        while time.monotonic() - T0 < args.duration:
            try:
                c, addr = ls.accept()
            except TimeoutError:
                continue
            n += 1
            log(f"server ACCEPT #{n}")
            threading.Thread(
                target=run_conn, args=(c, f"conn{n}", args, n == 1), daemon=True
            ).start()
        ls.close()
        time.sleep(0.6)
    if not RESULT["conns"]:
        sys.exit("FAIL no connection")
    if args.send and not RESULT["sent_all"]:
        sys.exit("FAIL not every --send size went out")
    if args.expect_reject and not RESULT["rejected"]:
        sys.exit("FAIL the peer kept the connection")
    if args.oversize_after and not RESULT["over_closed"]:
        sys.exit("FAIL the peer did not close after the oversize header")


if __name__ == "__main__":
    main()
