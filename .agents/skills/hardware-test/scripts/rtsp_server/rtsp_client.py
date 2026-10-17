"""Raw RTSP/TCP client for the esphome rtsp_server hardware test.

usage: rtsp_client.py HOST [case ...]
cases: play, udp, pipeline, third, stall, teardown, getparam, badreq, replay, timeout
"""

import socket
import struct
import sys
import time

HOST = sys.argv[1]
PORT = 554
cases = sys.argv[2:] or [
    "play",
    "udp",
    "pipeline",
    "badreq",
    "replay",
    "getparam",
    "teardown",
    "third",
    "stall",
]
results = []


def say(case, ok, detail):
    results.append((case, ok, detail))
    print(f"{'PASS' if ok else 'FAIL'} {case}: {detail}", flush=True)


class Client:
    def __init__(self, timeout=5):
        self.s = socket.create_connection((HOST, PORT), timeout=timeout)
        self.cseq = 0
        self.session = None
        self.buf = b""

    def send(self, method, extra="", url=None):
        self.cseq += 1
        url = (
            url or f"rtsp://{HOST}:{PORT}/trackID=0"
            if method == "SETUP"
            else (url or f"rtsp://{HOST}:{PORT}/")
        )
        req = f"{method} {url} RTSP/1.0\r\nCSeq: {self.cseq}\r\n"
        if self.session:
            req += f"Session: {self.session}\r\n"
        req += extra + "\r\n"
        self.s.sendall(req.encode())
        return self.cseq

    def read_response(self):
        # Read until a full RTSP response (headers + optional body); skip any interleaved data in between
        while True:
            if self.buf.startswith(b"$"):
                if len(self.buf) < 4:
                    self.buf += self.s.recv(4096)
                    continue
                ln = struct.unpack(">H", self.buf[2:4])[0]
                if len(self.buf) < 4 + ln:
                    self.buf += self.s.recv(4096)
                    continue
                self.buf = self.buf[4 + ln :]
                continue
            end = self.buf.find(b"\r\n\r\n")
            if end < 0:
                chunk = self.s.recv(4096)
                if not chunk:
                    raise ConnectionError("closed")
                self.buf += chunk
                continue
            head = self.buf[:end].decode(errors="replace")
            body_len = 0
            for line in head.split("\r\n"):
                if line.lower().startswith("content-length:"):
                    body_len = int(line.split(":", 1)[1])
            while len(self.buf) < end + 4 + body_len:
                self.buf += self.s.recv(4096)
            body = self.buf[end + 4 : end + 4 + body_len]
            self.buf = self.buf[end + 4 + body_len :]
            status = int(head.split(" ", 2)[1])
            for line in head.split("\r\n"):
                if line.lower().startswith("session:"):
                    self.session = line.split(":", 1)[1].strip().split(";")[0]
            return status, head, body

    def read_frames(self, seconds):
        """Collect interleaved RTP packets on channel 0 for `seconds`; return (frames, packets, seq_gaps, sizes)."""
        deadline = time.monotonic() + seconds
        frames = 0
        packets = 0
        gaps = 0
        last_seq = None
        cur = 0
        sizes = []
        self.s.settimeout(1.0)
        while time.monotonic() < deadline:
            try:
                chunk = self.s.recv(65536)
            except TimeoutError:
                continue
            if not chunk:
                break
            self.buf += chunk
            while True:
                if len(self.buf) < 4 or self.buf[0:1] != b"$":
                    if self.buf and self.buf[0:1] != b"$":
                        end = self.buf.find(b"\r\n\r\n")
                        if end < 0:
                            break
                        self.buf = self.buf[end + 4 :]
                        continue
                    break
                ch = self.buf[1]
                ln = struct.unpack(">H", self.buf[2:4])[0]
                if len(self.buf) < 4 + ln:
                    break
                pkt = self.buf[4 : 4 + ln]
                self.buf = self.buf[4 + ln :]
                if ch != 0 or len(pkt) < 12:
                    continue
                packets += 1
                seq = struct.unpack(">H", pkt[2:4])[0]
                marker = pkt[1] & 0x80
                if last_seq is not None and seq != ((last_seq + 1) & 0xFFFF):
                    gaps += 1
                last_seq = seq
                cur += len(pkt) - 12
                if marker:
                    frames += 1
                    sizes.append(cur)
                    cur = 0
        return frames, packets, gaps, sizes

    def close(self):
        self.s.close()


def setup_play(c):
    c.send("OPTIONS")
    st, head, _ = c.read_response()
    assert st == 200, head
    c.send("DESCRIBE", "Accept: application/sdp\r\n")
    st, head, body = c.read_response()
    assert st == 200, head
    c.send("SETUP", "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n")
    st, head, _ = c.read_response()
    assert st == 200, head
    c.send("PLAY")
    st, head, _ = c.read_response()
    assert st == 200, head
    return body


for case in cases:
    try:
        if case == "play":
            c = Client()
            sdp = setup_play(c)
            frames, packets, gaps, sizes = c.read_frames(10)
            c.send("TEARDOWN")
            c.read_response()
            c.close()
            ok = frames >= 50 and gaps == 0
            say(
                case,
                ok,
                f"{frames} frames in 10 s ({frames / 10:.1f} fps), {packets} RTP packets, {gaps} seq gaps, frame size min {min(sizes) if sizes else 0} max {max(sizes) if sizes else 0}; sdp has m=video {'m=video' in sdp.decode()}",
            )
        elif case == "udp":
            c = Client()
            c.send("OPTIONS")
            c.read_response()
            c.send("SETUP", "Transport: RTP/AVP;unicast;client_port=5000-5001\r\n")
            st, head, _ = c.read_response()
            c.close()
            say(case, st == 461, f"UDP SETUP answered {st}")
        elif case == "pipeline":
            c = Client()
            c.s.sendall(
                f"OPTIONS rtsp://{HOST}/ RTSP/1.0\r\nCSeq: 1\r\n\r\nDESCRIBE rtsp://{HOST}/ RTSP/1.0\r\nCSeq: 2\r\nAccept: application/sdp\r\n\r\n".encode()
            )
            c.cseq = 2
            st1, h1, _ = c.read_response()
            st2, h2, b2 = c.read_response()
            c.close()
            cs = [x for x in (h1 + h2).split("\r\n") if x.startswith("CSeq")]
            say(
                case,
                st1 == 200 and st2 == 200 and cs == ["CSeq: 1", "CSeq: 2"],
                f"two pipelined requests answered {st1}, {st2} with {cs}",
            )
        elif case == "badreq":
            c = Client()
            c.s.sendall(b"BOGUS rtsp://x/ RTSP/1.0\r\nCSeq: 7\r\n\r\n")
            st, head, _ = c.read_response()
            c.close()
            say(
                case,
                st in (400, 405, 501) and "CSeq: 7" in head,
                f"unknown method answered {st}, CSeq echoed {'CSeq: 7' in head}",
            )
        elif case == "replay":
            c = Client()
            setup_play(c)
            f1, _, _, _ = c.read_frames(3)
            c.send("PLAY")
            st, head, _ = c.read_response()
            f2, _, gaps, _ = c.read_frames(3)
            c.send("TEARDOWN")
            c.read_response()
            c.close()
            say(
                case,
                st == 200 and f2 >= 10 and gaps == 0,
                f"second PLAY answered {st}, {f1} then {f2} frames, {gaps} seq gaps after repeat PLAY",
            )
        elif case == "getparam":
            c = Client()
            setup_play(c)
            c.read_frames(1)
            c.send("GET_PARAMETER")
            st, head, _ = c.read_response()
            f, _, _, _ = c.read_frames(2)
            c.send("TEARDOWN")
            c.read_response()
            c.close()
            say(
                case,
                st == 200 and f > 5,
                f"GET_PARAMETER while playing answered {st}, stream continued with {f} frames in 2 s",
            )
        elif case == "teardown":
            c = Client()
            setup_play(c)
            c.read_frames(2)
            c.send("TEARDOWN")
            st, head, _ = c.read_response()
            c.s.settimeout(3)
            time.sleep(1.5)
            try:
                rest = c.s.recv(65536)
                closed = rest == b""
            except TimeoutError:
                rest = b"x"
                closed = False
            except ConnectionError:
                closed = True
            c.close()
            say(
                case,
                st == 200,
                f"TEARDOWN answered {st}; connection closed by server afterwards: {closed}",
            )
        elif case == "third":
            a = Client()
            setup_play(a)
            b = Client()
            setup_play(b)
            fa, _, ga, _ = a.read_frames(4)
            fb, _, gb, _ = b.read_frames(4)
            try:
                d = Client(timeout=3)
                d.send("OPTIONS")
                st, _, _ = d.read_response()
                third = f"third got {st}"
                d.close()
            except Exception as e:  # noqa: BLE001
                third = f"third rejected ({type(e).__name__})"
            a.send("TEARDOWN")
            a.read_response()
            a.close()
            b.send("TEARDOWN")
            b.read_response()
            b.close()
            say(
                case,
                fa > 10 and fb > 10 and ga == 0 and gb == 0 and "rejected" in third,
                f"two clients: {fa} and {fb} frames in 4 s each, gaps {ga}/{gb}; {third}",
            )
        elif case == "stall":
            a = Client()
            setup_play(a)
            b = Client()
            setup_play(b)
            # stop reading on a for 4 s while b keeps playing
            a.s.settimeout(None)
            fb1, _, gb1, _ = b.read_frames(4)
            a.s.settimeout(1.0)
            fa, _, ga, _ = a.read_frames(4)
            fb2, _, gb2, _ = b.read_frames(3)
            a.send("TEARDOWN")
            a.read_response()
            a.close()
            b.send("TEARDOWN")
            b.read_response()
            b.close()
            say(
                case,
                fb1 > 10 and fb2 > 10 and fa > 5,
                f"b kept {fb1} then {fb2} frames while a stalled 4 s; a recovered {fa} frames after resuming (gaps {ga}); b gaps {gb1}/{gb2}; device log must show stalled/dropping frame",
            )
        elif case == "timeout":
            c = Client(timeout=70)
            setup_play(c)
            c.s.settimeout(None)
            t0 = time.monotonic()
            dead = False
            try:
                while time.monotonic() - t0 < 68:
                    d = c.s.recv(65536)
                    if not d:
                        dead = True
                        break
            except ConnectionError:
                dead = True
            say(
                case,
                True,
                f"connection closed by server after idle: {dead} at {time.monotonic() - t0:.0f} s (expect ~60 s session timeout if the client sends nothing)",
            )
    except Exception as e:  # noqa: BLE001
        say(case, False, f"exception {type(e).__name__}: {e}")
    time.sleep(1)

print(
    "SUMMARY",
    sum(1 for r in results if r[1]),
    "pass",
    sum(1 for r in results if not r[1]),
    "fail",
)
