"""Announce with start_conversation from a URL whose first byte is DELAY seconds late; print the device timeline.

usage: drive.py HOST KEY DELAY OUT [SECONDS]   env: FLAC (file to serve, default tone.flac), SRV_PORT (default 18090)

Serves FLAC on SRV_PORT (any path), presses the "hwtest slow announce" button from slow-announce.yaml, whose URL
points here, and writes the HTTP events and device logs with times from the press to OUT. Exits 1 when the device
never fetched the URL.
"""

import asyncio
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
import pathlib
import re
import socket
import sys
import threading
import time

from aioesphomeapi import APIClient
from aioesphomeapi.model import LogLevel

HOST, KEY, DELAY, OUT = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4]
WAIT = float(sys.argv[5]) if len(sys.argv) > 5 else 30
BODY = pathlib.Path(os.environ.get("FLAC", "tone.flac")).read_bytes()
T0: float | None = None
ev: list[tuple[float, str]] = []


def mark(what: str) -> None:
    ev.append((time.monotonic() - (T0 or time.monotonic()), what))


class Handler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        mark(f"HTTP request {self.path}")
        time.sleep(DELAY)
        self.send_response(200)
        self.send_header("Content-Type", "audio/flac")
        self.send_header("Content-Length", str(len(BODY)))
        self.end_headers()
        mark("HTTP body sent")
        self.wfile.write(BODY)

    def log_message(self, *a: object) -> None:
        """Quiet: mark() records the requests."""


srv = ThreadingHTTPServer(
    ("0.0.0.0", int(os.environ.get("SRV_PORT", "18090"))), Handler
)
threading.Thread(target=srv.serve_forever, daemon=True).start()


async def main() -> None:
    global T0  # noqa: PLW0603
    cli = APIClient(
        socket.gethostbyname(HOST), 6053, None, noise_psk=None if KEY == "-" else KEY
    )
    await cli.connect(login=True)

    def on_log(msg) -> None:
        line = re.sub(r"\x1b\[[0-9;]*m", "", msg.message.decode(errors="replace"))
        if "runtime_stats" not in line:
            mark("LOG " + line[:150])

    cli.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
    await asyncio.sleep(1)
    ents, _ = await cli.list_entities_services()
    key = next(e.key for e in ents if e.object_id == "hwtest_slow_announce")
    T0 = time.monotonic()
    mark(f"PRESS announce button, url delay {DELAY}s")
    cli.button_command(key)
    await asyncio.sleep(WAIT + 12)
    await cli.disconnect()


asyncio.run(main())
srv.shutdown()
text = "".join(f"{t:7.2f}  {w}\n" for t, w in sorted(ev))
pathlib.Path(OUT).write_text(text, encoding="utf-8")
print(text)
if not any(w.startswith("HTTP request") for _, w in ev):
    sys.exit("FAIL: the device never fetched the announcement URL")
