"""Tests for esphome.stacktrace."""

from __future__ import annotations

from unittest.mock import Mock, patch

import pytest

from esphome import stacktrace
from esphome.core import EsphomeError

CONFIG = {"esphome": {"name": "test"}}

# Real dump lines per registered platform. "addresses" must fire the
# gate or that platform's decoding silently never starts. "markers" are
# the address-free lines a state-gated decoder (rp2, nrf52, esp8266)
# needs delivered first: their decoders only act once backtrace_state is
# set by the marker, so the replay buffer is what makes them work at
# all. A new decoder must declare its marker here so the delivery is
# pinned instead of discovered in the field.
CRASH_SAMPLES: dict[str, dict[str, list[str]]] = {
    "esp32": {
        "markers": [],
        "addresses": [
            "Backtrace: 0x400d1a2c:0x3ffb1f60 0x400d2a3c:0x3ffb1f80",
            "PC      : 0x400d1a2c  PS      : 0x00060330",
            "BT0: 0x40104960",
        ],
    },
    "esp8266": {
        "markers": [">>>stack>>>"],
        "addresses": [
            "epc1=0x40201234 epc2=0x00000000",
            "3ffffe10: 40201234 3ffe8410 00000000 40201000",
        ],
    },
    "rp2": {
        "markers": ["CRASH DETECTED ON PREVIOUS BOOT"],
        "addresses": ["PC:  0x10001234 (fault location)"],
    },
    "nrf52": {
        "markers": ["Last crash:"],
        "addresses": ["PC=0x27a1c LR=0x1e33"],
    },
}

CRASH_LINES = [
    pytest.param(line, id=f"{platform}-{n}")
    for platform, samples in CRASH_SAMPLES.items()
    for n, line in enumerate(samples["addresses"])
]

BENIGN_LINES = [
    pytest.param("[I][app:100] hello world", id="plain"),
    pytest.param("[C][wifi:400]   BSSID: AA:BB:CC:DD:EE:FF", id="mac"),
    pytest.param("[19:26:11.966][I][main:151]: version 2026.7.0-dev", id="timestamp"),
    pytest.param("[I][app:102]: Uptime: 12345678 ms", id="decimal-uptime"),
    pytest.param(
        "[D][sensor:093]: 'Water meter': Sending state 12345678.00000 L",
        id="decimal-sensor",
    ),
]


def test_crash_samples_cover_registry() -> None:
    """A newly registered decoder must come with a gate sample."""
    from esphome.platform_hooks import PLATFORM_HOOKS

    assert set(CRASH_SAMPLES) == set(PLATFORM_HOOKS["process_stacktrace"])


@pytest.mark.parametrize("line", CRASH_LINES)
def test_address_gate_fires_on_platform_dump_lines(line: str) -> None:
    assert stacktrace._ADDRESS_RE.search(line)


@pytest.mark.parametrize("platform", sorted(CRASH_SAMPLES))
def test_markers_reach_the_decoder_before_addresses(platform: str) -> None:
    """The replay buffer delivers each platform's markers in order.

    For state-gated decoders the marker is load-bearing: without it the
    address lines decode to nothing. This fails if a marker outgrows the
    replay window or the delivery order breaks.
    """
    markers = CRASH_SAMPLES[platform]["markers"]
    addresses = CRASH_SAMPLES[platform]["addresses"]
    handler = Mock(return_value=True)

    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, platform)
        for line in markers + addresses:
            processor.process_line(line)

    assert [call.args[1] for call in handler.call_args_list] == markers + addresses


@pytest.mark.parametrize("line", BENIGN_LINES)
def test_address_gate_ignores_ordinary_lines(line: str) -> None:
    assert not stacktrace._ADDRESS_RE.search(line)


def _make_processor(handler) -> stacktrace.LogLineProcessor:
    """Processor for a decoder platform with the resolver stubbed out."""
    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, "esp32")
        # Trigger lazy resolution while the stub is in place.
        processor.process_line("PC: 0x4010496e")
    return processor


def test_decoder_swallows_esphome_error() -> None:
    """A failing stack-trace decode must not propagate.

    aioesphomeapi isolates exceptions raised by log handlers, so an
    escaping one logs a full traceback for every line it fires on rather
    than being reported once as an unavailable decoder.
    """
    handler = Mock(side_effect=EsphomeError("no idedata"))
    processor = _make_processor(handler)

    assert handler.called
    assert processor.backtrace_state is False


def test_decoder_swallows_non_esphome_error() -> None:
    """Decoding failures that aren't EsphomeError must be contained too.

    A missing build directory surfaces as FileNotFoundError from the toolchain
    subprocess. aioesphomeapi isolates it, so the session survives, but it logs
    a traceback for every PC/BT line and decoding is never disabled, which
    buries the crash dump the user is trying to read.
    """
    handler = Mock(
        side_effect=FileNotFoundError(2, "No such file or directory", "/build")
    )
    processor = _make_processor(handler)
    processor.process_line("BT0: 0x4010496e")

    # Disabled after the first failure rather than retried per backtrace line.
    assert handler.call_count == 1
    assert processor.backtrace_state is False


def test_decoder_warning_uses_fallback_for_empty_error(caplog) -> None:
    """_run_idedata raises EsphomeError with no message; the warning
    must show a useful explanation rather than empty parens.
    """
    _make_processor(Mock(side_effect=EsphomeError()))

    warnings = [r.message for r in caplog.records if r.levelname == "WARNING"]
    assert any("build artifacts not found locally" in m for m in warnings)
    assert not any("()" in m for m in warnings)


def test_decoder_short_circuits_after_failure() -> None:
    """After one failure, subsequent lines must not retry the decoder.

    _decode_pc shells out to the toolchain; a crash dump can contain many
    PC/BT lines and retrying the failing subprocess for each one would
    stall log streaming.
    """
    handler = Mock(side_effect=EsphomeError("no idedata"))
    processor = _make_processor(handler)
    processor.process_line("BT0: 0x4010496e")
    processor.process_line("BT1: 0x401049aa")

    assert handler.call_count == 1


def test_decoder_failure_during_replay_short_circuits(caplog) -> None:
    """The first failure stops the replay too, not just later lines.

    The replay window is up to 8 lines; without the short circuit a
    single broken build would retry the failing toolchain subprocess for
    every buffered line and bury the dump under warnings.
    """
    handler = Mock(side_effect=EsphomeError("no idedata"))
    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, "esp32")
        processor.process_line("context line one")
        processor.process_line("context line two")
        processor.process_line("PC: 0x4010496e")

    # Only the first replayed context line was attempted.
    assert handler.call_count == 1
    warnings = [r for r in caplog.records if r.levelname == "WARNING"]
    assert len(warnings) == 1
    assert processor.backtrace_state is False


def test_decoder_replays_context_and_threads_state() -> None:
    """Markers without an address replay in order with state threaded.

    esp8266's ``>>>stack>>>`` region marker carries no address; the
    decoder must see it (entering its dump region) before the stack
    words that triggered resolution.
    """
    handler = Mock(side_effect=[True, True])
    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, "esp8266")
        processor.process_line(">>>stack>>>")
        processor.process_line("3ffffe10: 40201234 3ffe8410 00000000 40201000")

    assert [call.args[1] for call in handler.call_args_list] == [
        ">>>stack>>>",
        "3ffffe10: 40201234 3ffe8410 00000000 40201000",
    ]
    # The marker was fed with the initial state; the trigger line saw the
    # state the marker's call returned.
    assert handler.call_args_list[0].args[2] is False
    assert handler.call_args_list[1].args[2] is True
    assert processor.backtrace_state is True


def test_replay_buffer_is_bounded() -> None:
    """Only the most recent lines replay; a long quiet session stays flat."""
    handler = Mock(return_value=False)
    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, "esp32")
        for n in range(12):
            processor.process_line(f"quiet line {n}")
        processor.process_line("PC: 0x4010496e")

    fed = [call.args[1] for call in handler.call_args_list]
    assert fed == [f"quiet line {n}" for n in range(4, 12)] + ["PC: 0x4010496e"]


def test_processor_resolves_lazily_on_address_token() -> None:
    """No resolution attempt until a line carries an address token."""
    handler = Mock(return_value=False)

    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ) as mock_resolve:
        processor = stacktrace.LogLineProcessor(CONFIG, "esp32")
        processor.process_line("[I][app:100] hello world")
        processor.process_line("[I][wifi:200] connected")
        mock_resolve.assert_not_called()

        processor.process_line("PC: 0x40104960")
        mock_resolve.assert_called_once_with("esp32")

        # Later lines feed the resolved handler directly, no re-resolution.
        processor.process_line("[I][app:101] back to normal")
        mock_resolve.assert_called_once()

    assert [call.args[1] for call in handler.call_args_list] == [
        "[I][app:100] hello world",
        "[I][wifi:200] connected",
        "PC: 0x40104960",
        "[I][app:101] back to normal",
    ]


def test_processor_unexpected_resolution_error_disables_decoding(caplog) -> None:
    """Resolution is inside the containment guarantee like everything else."""
    with patch.object(
        stacktrace.platform_hooks,
        "get_stacktrace_handler",
        side_effect=OSError("filesystem went away"),
    ) as mock_resolve:
        processor = stacktrace.LogLineProcessor(CONFIG, "esp32")
        processor.process_line("PC: 0x40104960")
        processor.process_line("BT0: 0x40104960")

    mock_resolve.assert_called_once()
    warnings = [r for r in caplog.records if r.levelname == "WARNING"]
    assert len(warnings) == 1
    assert "could not be loaded" in warnings[0].message
    assert processor.backtrace_state is False


def test_letter_free_bad_alloc_reaches_resolved_decoder() -> None:
    """The one letter-free bare-hex form rides an already-open gate.

    ``last failed alloc call: 40201234(512)`` alone would not fire the
    gate, but it only appears inside a postmortem whose earlier lines
    (epc1=0x...) already resolved the decoder, so it feeds directly.
    """
    bad_alloc = "last failed alloc call: 40201234(512)"
    assert not stacktrace._ADDRESS_RE.search(bad_alloc)

    handler = Mock(return_value=False)
    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, "esp8266")
        processor.process_line("epc1=0x40201234 epc2=0x00000000")
        processor.process_line(bad_alloc)

    assert [call.args[1] for call in handler.call_args_list] == [
        "epc1=0x40201234 epc2=0x00000000",
        bad_alloc,
    ]


def test_decoder_bug_with_empty_message_names_the_type(caplog) -> None:
    """A zero-message decoder bug must not masquerade as missing artifacts.

    The recompile hint is only right for EsphomeError from _run_idedata;
    anything else would send the user down a dead-end remediation path.
    """
    _make_processor(Mock(side_effect=IndexError()))

    warnings = [r.message for r in caplog.records if r.levelname == "WARNING"]
    assert any("IndexError" in m for m in warnings)
    assert not any("build artifacts not found locally" in m for m in warnings)


def test_external_platform_notice_defers_to_first_crash_line(caplog) -> None:
    """External platforms lose the session-start notice by design.

    may_provide_hook cannot answer for a platform outside Platform
    without importing it, so nothing resolves at construction and the
    unavailable notice appears on the first crash-shaped line instead.
    """
    caplog.set_level("INFO", logger="esphome.platform_hooks")
    module = type("ExternalPlatform", (), {})  # no process_stacktrace

    with patch.object(
        stacktrace.platform_hooks.importlib,
        "import_module",
        Mock(return_value=module),
    ) as mock_import:
        processor = stacktrace.LogLineProcessor(CONFIG, "my_external_chip")
        # Construction cannot answer without importing, so it does neither.
        mock_import.assert_not_called()
        assert "Stacktrace analysis is unavailable" not in caplog.text

        processor.process_line("PC: 0x40104960")

    mock_import.assert_called_once()
    assert "Stacktrace analysis is unavailable" in caplog.text
    assert processor.backtrace_state is False


def test_processor_import_failure_disables_decoding(caplog) -> None:
    """A broken platform package degrades once instead of raising."""
    caplog.set_level("INFO", logger="esphome.platform_hooks")

    with patch.object(
        stacktrace.platform_hooks.importlib,
        "import_module",
        Mock(side_effect=ImportError("broken install")),
    ) as mock_import:
        processor = stacktrace.LogLineProcessor(CONFIG, "esp32")
        processor.process_line("PC: 0x40104960")
        processor.process_line("BT0: 0x40104960")

    mock_import.assert_called_once()
    assert "Stacktrace analysis is unavailable" in caplog.text
    assert "broken install" in caplog.text
    assert processor.backtrace_state is False


def test_processor_registry_miss_disables_at_construction(caplog) -> None:
    """Platforms the registry proves have no analyzer disable up front.

    The unavailable notice fires at session start (as it always did) and
    the per-line gate never runs.
    """
    caplog.set_level("INFO", logger="esphome.platform_hooks")

    with patch.object(
        stacktrace.platform_hooks.importlib, "import_module"
    ) as mock_import:
        processor = stacktrace.LogLineProcessor(CONFIG, "bk72xx")
        processor.process_line("PC: 0x40104960")

    mock_import.assert_not_called()
    assert "Stacktrace analysis is unavailable" in caplog.text
    assert processor.backtrace_state is False
