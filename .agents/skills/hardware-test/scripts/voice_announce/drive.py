"""Announce with start_conversation from a URL whose first byte is DELAY seconds late; print the device timeline.

usage: drive.py HOST KEY DELAY OUT [SECONDS]   env: FLAC (file to serve, default tone.flac), SRV_PORT (default 18090)

Serves FLAC on SRV_PORT (any path), presses the "hwtest slow announce" button from slow-announce.yaml, whose URL
points here, and writes the HTTP events and device logs with times from the press to OUT. It holds the voice
assistant subscription throughout, since without a subscriber the device drops the announcement state at once.
Exits 1 when the device never fetched the URL, the body send failed, an error was logged or no follow-up microphone
start came after the body.
"""

import asyncio
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
import pathlib
import sys
import threading
import time

from aioesphomeapi.model import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).parent.parent / "voice_wake_word_audio"))
from va_common import connect, log_text  # noqa: E402

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
        try:
            self.wfile.write(BODY)
            self.wfile.flush()
        except OSError as err:
            mark(f"HTTP body failed: {err!r}")
            return
        mark("HTTP body sent")

    def log_message(self, *a: object) -> None:
        """Quiet: mark() records the requests."""


srv = ThreadingHTTPServer(
    ("0.0.0.0", int(os.environ.get("SRV_PORT", "18090"))), Handler
)
threading.Thread(target=srv.serve_forever, daemon=True).start()


async def main() -> None:
    global T0  # noqa: PLW0603
    cli = await connect(HOST, KEY)

    def on_log(msg) -> None:
        line = log_text(msg)
        if "runtime_stats" not in line:
            mark("LOG " + line[:150])

    async def start(*a: object) -> int:
        mark("VA start (follow-up microphone)")
        return 0

    async def stop(abort: bool) -> None:
        mark(f"VA stop abort={abort}")

    async def audio(d1: bytes, d2: bytes) -> None:
        """Audio is not needed here."""

    cli.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
    cli.subscribe_voice_assistant(
        handle_start=start, handle_stop=stop, handle_audio=audio
    )
    await asyncio.sleep(1)
    ents, _ = await cli.list_entities_services()
    key = next((e.key for e in ents if e.object_id == "hwtest_slow_announce"), None)
    if key is None:
        await cli.disconnect()
        sys.exit(
            "FAIL: no hwtest_slow_announce button (slow-announce.yaml not in the config?)"
        )
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
if failed := [w for _, w in ev if w.startswith("HTTP body failed")]:
    sys.exit(f"FAIL: {failed[0]}")
if errors := [w for _, w in ev if w.startswith("LOG") and "[E]" in w]:
    sys.exit(f"FAIL: error logged: {errors[0]}")
sent = [t for t, w in ev if w == "HTTP body sent"]
if not sent:
    sys.exit("FAIL: the announcement body was never sent")
if not any(w == "VA start (follow-up microphone)" and t > sent[0] for t, w in ev):
    sys.exit("FAIL: no follow-up microphone start after the body was sent")
