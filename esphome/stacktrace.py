"""Lazy stack-trace decoding for streamed device log lines.

Shared by the serial (run_miniterm) and network (api_client) log paths.
Deliberately light: importing this module must not pull in aioesphomeapi
or any platform package.
"""

from __future__ import annotations

import logging
import re
from typing import Any

from esphome import platform_hooks
from esphome.core import EsphomeError

_LOGGER = logging.getLogger(__name__)

# The gate that defers the platform-package import: a log line reaches
# the decoder only once a line matches, so a session that never prints
# a crash pays no import at all. The win is one import per session
# instead of at session start, not per-line CPU. A false trigger costs
# that import on the event loop mid-stream, which is why 8-digit
# decimals (uptime counters) and ESP-IDF's decimal log timestamps must
# stay out. The device builder gates its decoder subprocess the same
# way.
#
# The gate must be a superset of every registered decoder's trigger
# language; both directions are pinned in
# tests/unit_tests/test_stacktrace.py, including a generative test that
# derives inputs from the decoder regexes themselves. Branches:
# 0x-prefixed pointers; bare 8-hex words that are not plain decimals;
# the letter-free register/alloc keyword forms, whose (?:0x)? covers a
# pointer glued to trailing word characters that defeat the first
# branch's \b; the exception header; and the region markers the
# state-gated decoders (rp2, nrf52, esp8266) key on, so those decoders
# never miss their marker line.
_ADDRESS_RE = re.compile(
    r"0x[0-9a-fA-F]{3,}\b"
    r"|\b(?![0-9]{8}\b)[0-9a-fA-F]{8}\b"
    r"|(?:PC|RA|MEPC|MTVAL|EXCVADDR|call)\s*[:=]\s*(?:0x)?4[0-9a-fA-F]{7}"
    r"|[eE]xception \(\d+\):"
    r"|>>>stack>>>|CRASH DETECTED ON PREVIOUS BOOT|Last crash:"
)


class LogLineProcessor:
    """Feeds incoming log lines to the stack-trace decoder.

    Three responsibilities beyond just calling the decoder:
    1. Resolve the platform decoder through the registry: lazily for
       in-tree platforms with a registered decoder, where nothing is
       imported until a line matches _ADDRESS_RE, and eagerly otherwise.
       A registry-proven miss reports its unavailable notice at session
       start without importing anything; an external platform resolves
       up front because the gate's grammar derives from the in-tree
       decoders and its import cannot be avoided anyway - resolving
       early keeps it out of the streaming callback, where a blocking
       import would stall delivery mid-stream.
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
        self.backtrace_state = False
        if not platform_hooks.has_registered_hook(platform, "process_stacktrace"):
            self._resolve_handler()

    def process_line(self, raw_line: str) -> None:
        if not self._decode_enabled:
            return
        if self._platform_handler is None:
            if not _ADDRESS_RE.search(raw_line):
                return
            # Deliberate trade: the platform import (~300 ms, seconds on
            # small hosts) blocks the streaming callback here, once per
            # session, instead of every session paying it at startup.
            if not self._resolve_handler():
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
            _LOGGER.debug("Stack-trace decoding failed", exc_info=True)
            if isinstance(exc, EsphomeError):
                # _run_idedata raises EsphomeError with no message; give
                # that case its friendly remediation hint.
                _LOGGER.warning(
                    "Crash trace decoding unavailable: %s. "
                    "Run 'esphome compile' for this device to enable PC decoding.",
                    str(exc) or "build artifacts not found locally",
                )
            else:
                # A decoder bug is ESPHome's problem, not the user's;
                # don't send them to recompile a healthy build.
                _LOGGER.warning(
                    'Crash trace decoding disabled: decoder for "%s" raised %s '
                    "(this is a bug; run with -v for the traceback)",
                    self._platform,
                    str(exc) or type(exc).__name__,
                )
