"""Tests for esphome.stacktrace."""

from __future__ import annotations

from itertools import chain, repeat
from unittest.mock import Mock, patch

from esphome import stacktrace
from esphome.const import PLATFORM_BK72XX, PLATFORM_ESP32, PLATFORM_ESP8266
from esphome.core import EsphomeError

CONFIG = {"esphome": {"name": "test"}}


def _run(
    handler,
    platform: str = PLATFORM_ESP32,
    lines: tuple[str, ...] = ("PC: 0x4010496e",),
) -> stacktrace.LogLineProcessor:
    """Processor with the resolver stubbed, fed the given lines."""
    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, platform)
    for line in lines:
        processor.process_line(line)
    return processor


def _fed(handler) -> list[str]:
    return [call.args[1] for call in handler.call_args_list]


def _warnings(caplog) -> list[str]:
    return [r.message for r in caplog.records if r.levelname == "WARNING"]


def _clock(*times: float):
    """Stub the module's monotonic, repeating the last value so an
    extra call cannot fail with StopIteration."""
    return patch.object(
        stacktrace, "monotonic", side_effect=chain(times, repeat(times[-1]))
    )


def test_decoder_contains_failures_and_short_circuits() -> None:
    """One decode failure is contained and not retried within the cooldown.

    aioesphomeapi isolates exceptions raised by log handlers, so an
    escaping one logs a full traceback for every line it fires on; and
    _decode_pc shells out to the toolchain, so retrying it per backtrace
    line would stall streaming.
    """
    handler = Mock(side_effect=EsphomeError("no idedata"))
    processor = _run(
        handler, lines=("PC: 0x4010496e", "BT0: 0x4010496e", "BT1: 0x401049aa")
    )

    assert handler.call_count == 1
    assert processor.backtrace_state is False


def test_latch_rearms_after_cooldown(caplog) -> None:
    """A transient failure must not kill decoding for the whole session.

    Within the cooldown lines are skipped; once it passes the next line
    gets a fresh decode attempt, and a failure with a new cause warns
    again instead of degrading silently.
    """
    handler = Mock(side_effect=[EsphomeError("no idedata"), EsphomeError("port busy")])
    with _clock(0.0, 30.0, 61.0):
        processor = _run(
            handler, lines=("PC: 0x4010496e", "BT0: 0x4010496e", "BT1: 0x401049aa")
        )

    assert handler.call_count == 2
    assert processor.backtrace_state is False
    assert len([m for m in _warnings(caplog) if "esphome compile" in m]) == 2


def test_identical_refailure_downgrades_to_debug(caplog) -> None:
    """A permanent cause must not warn once per cooldown for hours.

    The first failure warns in full; an identical re-failure after the
    cooldown logs at debug only.
    """
    handler = Mock(side_effect=EsphomeError("no idedata"))
    with _clock(0.0, 61.0):
        _run(handler, lines=("PC: 0x4010496e", "BT0: 0x4010496e"))

    assert handler.call_count == 2
    assert len([m for m in _warnings(caplog) if "esphome compile" in m]) == 1


def test_backoff_doubles_after_identical_refailure() -> None:
    """Identical re-failures back off exponentially.

    Every retry re-runs the failing toolchain subprocess, so a
    permanently broken environment must retry less and less often
    instead of hitching the stream once a minute for the session.
    """
    handler = Mock(side_effect=EsphomeError("no idedata"))
    with _clock(0.0, 61.0, 61.0, 100.0, 200.0, 200.0):
        _run(
            handler,
            lines=(
                "PC: 0x4010496e",  # fails; cooldown 60
                "BT0: 0x4010496e",  # t=61: re-arms, fails again; cooldown 120
                "BT1: 0x401049aa",  # t=100: 39s into the 120s cooldown, skipped
                "BT2: 0x401049aa",  # t=200: 139s elapsed, re-arms, fails
            ),
        )

    assert handler.call_count == 3


def test_ordinary_lines_do_not_end_the_failure_episode(caplog) -> None:
    """A non-raising return proves nothing about the environment.

    The handler is fed every log line and returns without decoding on
    ordinary ones, so treating that as recovery would reset the episode
    on the first chatty line and the backoff would never engage; a
    crash-looping device would warn and launch platformio once a minute
    for the whole session.
    """
    handler = Mock(
        side_effect=[EsphomeError("no idedata"), False, EsphomeError("no idedata")]
    )
    with _clock(0.0, 61.0, 61.0):
        _run(handler, lines=("PC: 0x4010496e", "BT0: 0x4010496e", "BT1: 0x401049aa"))

    assert handler.call_count == 3
    assert len([m for m in _warnings(caplog) if "esphome compile" in m]) == 1


def test_resolution_failure_is_contained(caplog) -> None:
    """A platform package broken in an unanticipated way must not kill
    the session; decoding degrades with a warning like any other failure.
    """
    with patch.object(
        stacktrace.platform_hooks,
        "get_stacktrace_handler",
        side_effect=RuntimeError("boom"),
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, PLATFORM_ESP32)
        processor.process_line("PC: 0x4010496e")

    assert processor.backtrace_state is False
    assert any("could not be loaded" in m for m in _warnings(caplog))


def test_no_analyzer_never_rearms(caplog) -> None:
    """A platform without an analyzer stays off; there is nothing to retry."""
    processor = _run(None, platform=PLATFORM_BK72XX, lines=())
    with _clock(1e9):
        processor.process_line("PC: 0x4010496e")

    assert processor.backtrace_state is False
    # A re-arm here would feed the missing handler and warn; it must not.
    assert not _warnings(caplog)


def test_decoder_swallows_os_error_with_remediation_hint(caplog) -> None:
    """Decoding failures that aren't EsphomeError must be contained too.

    A missing build directory surfaces as an OSError; that is the
    user's environment, not a decoder bug, so it disables decoding
    like an EsphomeError does and keeps the recompile hint.
    """
    handler = Mock(
        side_effect=FileNotFoundError(2, "No such file or directory", "/build")
    )
    processor = _run(handler, lines=("PC: 0x4010496e", "BT0: 0x4010496e"))

    assert handler.call_count == 1
    assert processor.backtrace_state is False
    warnings = _warnings(caplog)
    assert any("esphome compile" in m for m in warnings)
    assert not any("this is a bug" in m for m in warnings)


def test_decoder_warning_uses_fallback_for_empty_error(caplog) -> None:
    """A message-less EsphomeError must show a useful explanation.

    Defensive: the in-tree idedata raise sites all carry a message now,
    but a bare EsphomeError from elsewhere must not render as parens.
    """
    _run(Mock(side_effect=EsphomeError()))

    warnings = _warnings(caplog)
    assert any("build artifacts not found locally" in m for m in warnings)
    assert not any("()" in m for m in warnings)


def test_decoder_bug_with_empty_message_names_the_type(caplog) -> None:
    """A zero-message decoder bug must not masquerade as missing artifacts.

    The recompile hint is only right for EsphomeError from _run_idedata;
    anything else is ESPHome's own bug and says so instead of sending
    the user down a dead-end remediation path.
    """
    _run(Mock(side_effect=IndexError()))

    warnings = _warnings(caplog)
    assert any("IndexError" in m and "this is a bug" in m for m in warnings)
    assert not any("esphome compile" in m for m in warnings)


def test_decoder_bug_warning_keeps_the_type_with_a_message(caplog) -> None:
    """The type must survive a non-empty message; a bare KeyError message
    like 'prog_path' reads as a raised string in a bug report paste.
    """
    _run(Mock(side_effect=KeyError("prog_path")))

    warnings = _warnings(caplog)
    assert any("KeyError: 'prog_path'" in m for m in warnings)


def test_state_threads_between_lines() -> None:
    """backtrace_state carries from one decoded line to the next."""
    handler = Mock(side_effect=[True, True])
    processor = _run(
        handler,
        platform=PLATFORM_ESP8266,
        lines=(">>>stack>>>", "3ffffe10: 40201234 3ffe8410 00000000 40201000"),
    )

    assert _fed(handler) == [
        ">>>stack>>>",
        "3ffffe10: 40201234 3ffe8410 00000000 40201000",
    ]
    assert handler.call_args_list[0].args[2] is False
    assert handler.call_args_list[1].args[2] is True
    assert processor.backtrace_state is True


def test_no_analyzer_disables_decoding(caplog) -> None:
    """Platforms without an analyzer report at session start and stay quiet."""
    caplog.set_level("INFO", logger="esphome.platform_hooks")
    processor = stacktrace.LogLineProcessor(CONFIG, PLATFORM_BK72XX)
    processor.process_line("PC: 0x40104960")

    assert "Stacktrace analysis is unavailable" in caplog.text
    assert processor.backtrace_state is False
