"""Stream FLAC files chunked: everything up to the intro split, then hold the connection open, then the rest.

usage: srv.py PORT WWW_DIR   env: PAUSE (seconds, default 8)
Each file needs NAME.cut next to it (from mk.py). Logs REQ, INTRO_SENT and REST_SENT with epoch times to stdout.
"""

import http.server
import os
import pathlib
import socketserver
import sys
import time

PAUSE = float(os.environ.get("PAUSE", "8"))
WWW = pathlib.Path(sys.argv[2])


def log(*a: object) -> None:
    print(f"{time.time():.3f}", *a, flush=True)


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def do_GET(self) -> None:
        name = self.path.lstrip("/").split("?")[0]
        data = (WWW / name).read_bytes()
        cut = int((WWW / (name + ".cut")).read_text(encoding="utf-8").split()[0])
        log("REQ", self.path)
        self.send_response(200)
        self.send_header("Content-Type", "audio/flac")
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()

        def chunk(b: bytes) -> None:
            for i in range(0, len(b), 1400):
                c = b[i : i + 1400]
                self.wfile.write(b"%x\r\n" % len(c) + c + b"\r\n")
            self.wfile.flush()

        try:
            chunk(data[:cut])
            log("INTRO_SENT", cut)
            time.sleep(PAUSE)
            chunk(data[cut:])
            self.wfile.write(b"0\r\n\r\n")
            self.wfile.flush()
            log("REST_SENT", len(data))
        except OSError as e:
            log("ERR", e)

    def log_message(self, *a: object) -> None:
        """Quiet: the REQ lines are enough."""


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


Server(("0.0.0.0", int(sys.argv[1])), Handler).serve_forever()
