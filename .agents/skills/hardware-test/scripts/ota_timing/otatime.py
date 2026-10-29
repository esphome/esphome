"""Time one ESPHome OTA upload and print one RESULT json line.
Usage: otatime.py HOST IMAGE MODE   (MODE: deflate, raw, deflate-tcp or raw-tcp)
env: OTA_PORT (default 3232), OTA_PASSWORD, API_KEY (a board with an api encryption key also encrypts OTA),
TAG (label stored in the result)
Run it with the checkout under test first on PYTHONPATH, so its espota2 is the client being measured.
raw: the client leaves deflate out of its feature offer. Never patch the deflate branch off after the offer: once
the device accepted deflate it expects a deflate stream. -tcp: the link is reported clean, so the client asks to
stay on TCP. Clearing the UDP feature bit instead desyncs the handshake ("receiving UDP token response: timed out")."""

import json
import logging
import os
from pathlib import Path
import sys
import time

import esphome.espota2 as espota

host, image, mode = sys.argv[1], Path(sys.argv[2]), sys.argv[3]
# Assigning a missing attribute would silently do nothing, so a renamed name must stop the run
for name in ("CLIENT_FEATURE_SUPPORTS_DEFLATE", "_loss_reason"):
    if not hasattr(espota, name):
        sys.exit(f"espota2 in this tree has no {name}; update otatime.py")
if mode.startswith("raw"):
    espota.CLIENT_FEATURE_SUPPORTS_DEFLATE = 0
if mode.endswith("-tcp"):
    espota._loss_reason = lambda *a: None  # noqa: SLF001
marks: list[tuple[float, float]] = []


class Recorder:
    """Stands in for the progress bar and records when each fraction went out."""

    def __init__(self, *args) -> None:
        pass

    def update(self, progress: float) -> None:
        marks.append((time.perf_counter(), progress))

    def done(self) -> None:
        pass


espota.ProgressBar = Recorder
lines: list[str] = []


class Keep(logging.Handler):
    def emit(self, record: logging.LogRecord) -> None:
        lines.append(record.getMessage())


logging.basicConfig(level=logging.INFO)
logging.getLogger().addHandler(Keep())
t0 = time.perf_counter()
rc, _ = espota.run_ota(
    host,
    int(os.environ.get("OTA_PORT", "3232")),
    os.environ.get("OTA_PASSWORD"),
    image,
    noise_psk=os.environ.get("API_KEY"),
)
total = time.perf_counter() - t0
deciles = []
if marks:
    start = marks[0][0]
    for q in range(1, 11):
        hit = next((t for t, f in marks if f >= q / 10 - 1e-9), None)
        deciles.append(round(hit - start, 2) if hit is not None else None)
result = {
    "board": os.environ.get("TAG", ""),
    "mode": mode,
    "rc": rc,
    "total": round(total, 2),
    "took": next((x for x in lines if x.startswith("Update took")), ""),
    "size": next((x for x in lines if "Compressed to" in x), ""),
    "udp": any("Using UDP" in x for x in lines),
    "deciles": deciles,
}
# The log must agree with the mode, or the row would be counted under the wrong mode
if mode.startswith("raw") and result["size"]:
    result["rc"] = rc = rc or 1
    print("raw mode upload was compressed anyway", file=sys.stderr)
if mode.endswith("-tcp") and result["udp"]:
    result["rc"] = rc = rc or 1
    print("-tcp mode upload switched to UDP", file=sys.stderr)
print("RESULT", json.dumps(result), flush=True)
sys.exit(1 if rc else 0)
