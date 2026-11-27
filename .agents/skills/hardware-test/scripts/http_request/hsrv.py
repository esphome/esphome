"""Test server for http_request actions: deterministic bodies, slow and chunked responses."""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import sys
import time
from urllib.parse import parse_qs, urlparse


def body_bytes(n):
    return bytes((i * 7 + 3) & 0xFF for i in range(n))


class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *a):
        sys.stdout.write(f"{time.time():.3f} {self.client_address[0]} {fmt % a}\n")
        sys.stdout.flush()

    def _q(self):
        u = urlparse(self.path)
        return u.path, {k: v[0] for k, v in parse_qs(u.query).items()}

    def _send(self, data, code=200, ctype="application/octet-stream"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        got = self.rfile.read(n)
        self._send(
            b"posted %d %d" % (len(got), sum(got) & 0xFFFFFFFF), ctype="text/plain"
        )

    def do_GET(self):
        path, q = self._q()
        if path == "/fast":
            self._send(b"hello", ctype="text/plain")
        elif path == "/slow":
            time.sleep(float(q.get("d", "2")))
            self._send(b"slow ok", ctype="text/plain")
        elif path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/fast")
            self.send_header("Content-Length", "0")
            self.end_headers()
        elif path == "/hang":
            time.sleep(float(q.get("d", "30")))
        elif path in ("/big", "/chunked"):
            n = int(q.get("n", "70000"))
            rate = float(q.get("rate", "20000"))
            data = body_bytes(n)
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            if path == "/big":
                self.send_header("Content-Length", str(n))
            else:
                self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            step = 1000
            for i in range(0, n, step):
                part = data[i : i + step]
                if path == "/big":
                    self.wfile.write(part)
                else:
                    self.wfile.write(b"%x\r\n" % len(part) + part + b"\r\n")
                self.wfile.flush()
                time.sleep(len(part) / rate)
            if path == "/chunked":
                self.wfile.write(b"0\r\n\r\n")
        else:
            self._send(b"nope", 404)


ThreadingHTTPServer(("0.0.0.0", int(sys.argv[1])), H).serve_forever()
