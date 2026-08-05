"""Stack-trace decoding for streamed device log lines.

Shared by the serial (run_miniterm) and network (api_client) log paths.
Deliberately light: importing this module must not pull in aioesphomeapi
or any platform package.
"""

from __future__ import annotations

import logging
import time
from typing import TYPE_CHECKING

from esphome import platform_hooks
from esphome.core import EsphomeError
from esphome.types import ConfigType

if TYPE_CHECKING:
    from collections.abc import Callable

    # The contract every platform's process_stacktrace implements.
    StacktraceHandler = Callable[[ConfigType, str, bool], bool]

_LOGGER = logging.getLogger(__name__)

# How long a decode failure keeps decoding off. Long enough that a dump
# with many PC/BT lines fails once instead of retrying a failing
# addr2line per line, short enough that a transient cause (a concurrent
# compile rewriting the build tree, a momentary toolchain lock) does not
# blank out decoding for the rest of a multi-hour session. A permanent
# cause re-fails and re-warns at most once per cooldown.
_REARM_COOLDOWN_S = 60.0


class LogLineProcessor:
    """Feeds incoming log lines to the stack-trace decoder.

    Two responsibilities beyond just calling the decoder:
    1. Catch everything the decoder can raise. aioesphomeapi isolates
       exceptions raised by log handlers, so an escaping one no longer
       kills the session, but it does log a full traceback per line. A
       crash dump carries a PC line plus one per backtrace frame, so the
       tracebacks bury the dump the user is trying to read. Decoding is a
       diagnostic nicety; nothing it raises is worth that noise.
    2. Disable decoding after a failure, for _REARM_COOLDOWN_S. _decode_pc
       shells out to the toolchain to resolve addr2line, which is
       expensive; a single crash dump can contain many PC/BT lines and we
       don't want to retry the failing subprocess for each one. This only
       works if every failure is caught, which is why 1 is not narrowed
       to EsphomeError. A platform without an analyzer, or one whose
       package failed to load, stays off for the whole session; an
       import that just failed will not heal mid-stream, so there is
       nothing worth retrying.
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
        self._disabled_at: float | None = None
        self._last_failure: str | None = None
        self.backtrace_state = False

    def process_line(self, raw_line: str) -> None:
        if not self._decode_enabled and not self._rearm():
            return
        self._feed(raw_line)

    def _rearm(self) -> bool:
        """Give decoding another try once the failure cooldown has passed."""
        if self._disabled_at is None:  # no analyzer; nothing to retry
            return False
        if time.monotonic() - self._disabled_at < _REARM_COOLDOWN_S:
            return False
        self._decode_enabled = True
        self._disabled_at = None
        return True

    def _feed(self, raw_line: str) -> None:
        try:
            self.backtrace_state = self._platform_handler(
                self._config, raw_line, self.backtrace_state
            )
            # A success ends the failure episode; a later failure with
            # the same message is a new episode and warns in full again.
            self._last_failure = None
        except Exception as exc:  # noqa: BLE001  # pylint: disable=broad-except
            self._decode_enabled = False
            self._disabled_at = time.monotonic()
            self.backtrace_state = False
            _LOGGER.debug("Stack-trace decoding failed", exc_info=True)
            failure = f"{type(exc).__name__}: {exc}"
            if failure == self._last_failure:
                # A permanent cause re-fails on every re-arm; one full
                # warning is enough. Repeats go to debug so an hours-long
                # crash loop does not bury the dump in reminders, while a
                # changed failure still warns.
                _LOGGER.debug("Stack-trace decoding still failing: %s", failure)
                return
            self._last_failure = failure
            if isinstance(exc, (EsphomeError, OSError)):
                # _run_idedata raises EsphomeError with no message, and a
                # missing build tree surfaces as an OSError; both are the
                # user's environment, so give the remediation hint.
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
