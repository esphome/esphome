"""Stack-trace decoding for streamed device log lines.

Shared by the serial (run_miniterm) and network (api_client) log paths.
Deliberately light: importing this module must not pull in aioesphomeapi
or any platform package.
"""

from __future__ import annotations

import logging
from typing import TYPE_CHECKING

from esphome import platform_hooks
from esphome.core import EsphomeError
from esphome.types import ConfigType

if TYPE_CHECKING:
    from collections.abc import Callable

    # The contract every platform's process_stacktrace implements.
    StacktraceHandler = Callable[[ConfigType, str, bool], bool]

_LOGGER = logging.getLogger(__name__)


class LogLineProcessor:
    """Feeds incoming log lines to the stack-trace decoder.

    Two responsibilities beyond just calling the decoder:
    1. Catch everything the decoder can raise. aioesphomeapi isolates
       exceptions raised by log handlers, so an escaping one no longer
       kills the session, but it does log a full traceback per line. A
       crash dump carries a PC line plus one per backtrace frame, so the
       tracebacks bury the dump the user is trying to read. Decoding is a
       diagnostic nicety; nothing it raises is worth that noise.
    2. Disable decoding for the rest of the session after a failure.
       _decode_pc shells out to the toolchain to resolve addr2line,
       which is expensive; a single crash dump can contain many PC/BT
       lines and we don't want to retry the failing subprocess for each
       one. This only works if every failure is caught, which is why 1
       is not narrowed to EsphomeError. The latch is deliberately one
       way: nothing a decode failure depends on heals by itself within
       a session, the warning names the fix, and a fresh ``esphome
       logs`` run picks it up; retrying mid-session would block the
       stream with a failing subprocess instead.
    """

    def __init__(self, config: ConfigType, platform: str) -> None:
        self._config = config
        self._platform = platform
        self._platform_handler: StacktraceHandler | None
        try:
            self._platform_handler = platform_hooks.get_stacktrace_handler(platform)
        except Exception as exc:  # noqa: BLE001  # pylint: disable=broad-except
            # Total containment includes resolution: a platform package
            # broken in an unanticipated way must not kill the session.
            # Name the cause; the full traceback only exists at debug.
            _LOGGER.debug("Stacktrace analyzer resolution failed", exc_info=True)
            _LOGGER.warning(
                'Stacktrace analysis is unavailable: analyzer for target platform "%s" could not be loaded: %s',
                platform,
                f"{type(exc).__name__}: {exc}",
            )
            self._platform_handler = None
        self._decode_enabled = self._platform_handler is not None
        self.backtrace_state = False

    def process_line(self, raw_line: str) -> None:
        if not self._decode_enabled:
            return
        self._feed(raw_line)

    def _feed(self, raw_line: str) -> None:
        try:
            self.backtrace_state = self._platform_handler(
                self._config, raw_line, self.backtrace_state
            )
        except Exception as exc:  # noqa: BLE001  # pylint: disable=broad-except
            self._decode_enabled = False
            self.backtrace_state = False
            _LOGGER.debug("Stack-trace decoding failed", exc_info=True)
            if isinstance(exc, (EsphomeError, OSError)):
                # The environment branch: idedata and build tree failures
                # get the remediation hint. The fallback string is
                # defensive; the in-tree raise sites all carry a message
                # now, but a bare EsphomeError must not render as parens.
                _LOGGER.warning(
                    "Crash trace decoding unavailable: %s. "
                    "Run 'esphome compile' for this device to enable PC decoding.",
                    str(exc) or "build artifacts not found locally",
                )
            else:
                # A decoder bug is ESPHome's problem, not the user's;
                # don't send them to recompile a healthy build. Always
                # name the type: a bare KeyError message reads like a
                # raised string in the paste a bug report needs.
                detail = type(exc).__name__
                if msg := str(exc):
                    detail = f"{detail}: {msg}"
                _LOGGER.warning(
                    'Crash trace decoding disabled: decoder for "%s" raised %s '
                    "(this is a bug; run with -v for the traceback)",
                    self._platform,
                    detail,
                )
