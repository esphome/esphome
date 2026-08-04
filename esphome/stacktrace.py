"""Lazy stack-trace decoding for streamed device log lines.

Shared by the serial (run_miniterm) and network (api_client) log paths.
Deliberately light: importing this module must not pull in aioesphomeapi
or any platform package.
"""

from __future__ import annotations

from collections import deque
import logging
import re
from typing import Any

from esphome import platform_hooks

_LOGGER = logging.getLogger(__name__)


# Necessary condition for a line to decode to anything: every frame or
# register the platform decoders match carries either a 0x-prefixed
# pointer (nrf52's variable-length ``PC=0x27a1c``, esp32's
# ``Backtrace: 0x400d1a2c:...``) or a bare 8-hex-digit word (esp8266
# stack dumps). A unit test keyed off the registry feeds one real dump
# line per registered platform through this to keep the superset claim
# honest. Deliberately
# not the crash-marker grammar - that lives in the platform decoders
# and a copy here would drift. The device builder gates its decoder
# subprocess the same way (esphome_device_builder/controllers/devices/
# backtrace.py).
# The bare-hex alternation requires at least one letter so 8-digit
# decimals (uptime counters, sensor readings) don't trip it. Stack-dump
# lines always carry one via their 3ff... stack addresses; the one
# letter-free bare-hex form (a bad-alloc address like 40201234) appears
# only inside a postmortem whose earlier lines already opened the gate,
# so it reaches the decoder as a direct feed.
_ADDRESS_RE = re.compile(
    r"0x[0-9a-fA-F]{4,}\b|\b(?=[0-9A-Fa-f]*[A-Fa-f])[0-9a-fA-F]{8}\b"
)

# Lines kept for replay once decoding starts. Crash markers without an
# address (esp8266's ``>>>stack>>>``, panic banners) precede the first
# address-bearing line by at most a few lines; replaying them lets the
# decoder enter its dump region as if it had seen the stream live.
_REPLAY_LINES = 8


class LogLineProcessor:
    """Feeds incoming log lines to the stack-trace decoder.

    Three responsibilities beyond just calling the decoder:
    1. Resolve the platform decoder lazily. Importing a platform package
       pulls in the validation stack, so nothing is imported until a
       line carries something that could decode (see _ADDRESS_RE); the
       preceding context lines replay from a small buffer so multi-line
       dumps whose opening marker has no address still decode. Sessions
       that never see a crash-shaped line never pay the import.
    2. Catch everything the decoder can raise. aioesphomeapi isolates
       exceptions raised by log handlers, so an escaping one no longer
       kills the session, but it does log a full traceback per line. A
       crash dump carries a PC line plus one per backtrace frame, so the
       tracebacks bury the dump the user is trying to read. Decoding is a
       diagnostic nicety; nothing it raises is worth that noise.
    3. Disable decoding after the first failure. _decode_pc shells out to
       the toolchain to resolve addr2line, which is expensive; a single
       crash dump can contain many PC/BT lines and we don't want to retry
       the failing subprocess for each one. This only works if every
       failure is caught, which is why 2 is not narrowed to EsphomeError.
    """

    def __init__(self, config: dict[str, Any], platform: str) -> None:
        self._config = config
        self._platform = platform
        self._platform_handler: Any | None = None
        self._decode_enabled = True
        self._recent: deque[str] = deque(maxlen=_REPLAY_LINES)
        self.backtrace_state = False
        # The registry can prove in-tree platforms without an analyzer
        # need no import; resolve those now so their unavailable notice
        # fires at session start and the per-line gate never runs.
        # External platforms cannot be answered without importing, so
        # their notice defers to the first crash-shaped line.
        if not platform_hooks.may_provide_hook(platform, "process_stacktrace"):
            self._resolve_handler()

    def process_line(self, raw_line: str) -> None:
        if not self._decode_enabled:
            return
        if self._platform_handler is None:
            if not _ADDRESS_RE.search(raw_line):
                self._recent.append(raw_line)
                return
            if not self._resolve_handler():
                return
            pending = list(self._recent)
            self._recent.clear()
            for buffered in pending:
                self._feed(buffered)
                if not self._decode_enabled:
                    return
        self._feed(raw_line)

    def _resolve_handler(self) -> bool:
        try:
            handler = platform_hooks.get_stacktrace_handler(self._platform)
        except Exception:  # noqa: BLE001  # pylint: disable=broad-except
            # Total containment includes resolution: a platform package
            # broken in an unanticipated way must not kill the session or
            # retry on every address-bearing line.
            _LOGGER.debug("Stacktrace analyzer resolution failed", exc_info=True)
            _LOGGER.warning(
                'Stacktrace analysis is unavailable: analyzer for target platform "%s" could not be loaded.',
                self._platform,
            )
            handler = None
        if handler is None:
            self._decode_enabled = False
            return False
        self._platform_handler = handler
        return True

    def _feed(self, raw_line: str) -> None:
        try:
            self.backtrace_state = self._platform_handler(
                self._config, raw_line, self.backtrace_state
            )
        except Exception as exc:  # noqa: BLE001  # pylint: disable=broad-except
            self._decode_enabled = False
            self.backtrace_state = False
            # _run_idedata raises EsphomeError with no message; fall back
            # to a generic explanation when str(exc) is empty.
            detail = str(exc) or "build artifacts not found locally"
            _LOGGER.debug("Stack-trace decoding failed", exc_info=True)
            _LOGGER.warning(
                "Crash trace decoding unavailable: %s. "
                "Run 'esphome compile' for this device to enable PC decoding.",
                detail,
            )
