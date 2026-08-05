"""Lazy stack-trace decoding for streamed device log lines.

Shared by the serial (run_miniterm) and network (api_client) log paths.
Deliberately light: importing this module must not pull in aioesphomeapi
or any platform package.
"""

from __future__ import annotations

import logging
import re
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

    Three responsibilities beyond just calling the decoder:
    1. Resolve the platform decoder lazily: registered platforms import
       nothing until a line matches their gate, registry misses report
       at session start without importing, and external platforms
       resolve eagerly since their import is unavoidable and belongs
       off the streaming callback.
    2. Catch everything the decoder can raise; decoding is a diagnostic
       nicety and an escaping exception would log a traceback per dump
       line, burying the dump the user is trying to read.
    3. Disable decoding for the rest of the session after a failure.
       Retrying means re-running a failing toolchain subprocess on the
       stream, and nothing a decode failure depends on heals by itself;
       the warning names the fix and a fresh run picks it up. Working
       at all requires catching every failure, which is why 2 is not
       narrowed to EsphomeError.
    """

    def __init__(self, config: ConfigType, platform: str) -> None:
        self._config = config
        self._platform = platform
        self._platform_handler: StacktraceHandler | None = None
        self._decode_enabled = True
        # None only for platforms resolved eagerly below; a registered
        # platform always declares a gate.
        gate = platform_hooks.STACKTRACE_GATES.get(platform)
        self._gate: re.Pattern[str] | None = None if gate is None else re.compile(gate)
        self.backtrace_state = False
        if not platform_hooks.has_registered_hook(platform, "process_stacktrace"):
            self._resolve_handler()

    def process_line(self, raw_line: str) -> None:
        if not self._decode_enabled:
            return
        if self._platform_handler is None:
            if not self._gate.search(raw_line):
                return
            # Deliberate trade: the platform import blocks the stream
            # here, once per session, instead of at every startup.
            if not self._resolve_handler():
                return
            _LOGGER.debug(
                "Stacktrace gate fired for %s; decoder resolved", self._platform
            )
        self._feed(raw_line)

    def _resolve_handler(self) -> bool:
        try:
            handler = platform_hooks.get_stacktrace_handler(self._platform)
        except Exception as exc:  # noqa: BLE001  # pylint: disable=broad-except
            # Containment includes resolution: a broken platform package
            # must not kill the session or retry per line.
            _LOGGER.debug("Stacktrace analyzer resolution failed", exc_info=True)
            _LOGGER.warning(
                'Stacktrace analysis is unavailable: analyzer for target platform "%s" could not be loaded: %s',
                self._platform,
                f"{type(exc).__name__}: {exc}",
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
            if isinstance(exc, (EsphomeError, OSError)):
                # Environment failures (idedata, build tree) get the
                # remediation hint; the fallback string keeps a bare
                # EsphomeError from rendering as empty parens.
                _LOGGER.warning(
                    "Crash trace decoding unavailable: %s. "
                    "Run 'esphome compile' for this device to enable PC decoding.",
                    str(exc) or "build artifacts not found locally",
                )
            else:
                # A decoder bug is ESPHome's problem, not the user's;
                # don't send them to recompile a healthy build. Name the
                # type so a bare KeyError message reads as an exception.
                detail = type(exc).__name__
                if msg := str(exc):
                    detail = f"{detail}: {msg}"
                _LOGGER.warning(
                    'Crash trace decoding disabled: decoder for "%s" raised %s '
                    "(this is a bug; run with -v for the traceback)",
                    self._platform,
                    detail,
                )
