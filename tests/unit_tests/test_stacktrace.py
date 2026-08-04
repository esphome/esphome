"""Tests for esphome.stacktrace."""

from __future__ import annotations

from unittest.mock import Mock, patch

import pytest

from esphome import stacktrace
from esphome.core import EsphomeError

CONFIG = {"esphome": {"name": "test"}}

# Real dump lines per registered platform. "addresses" must fire the
# gate or that platform's decoding silently never starts.
# "state_markers" are the address-free lines that set backtrace_state;
# the gate must match them directly so their decoders never depend on
# the replay window. "context_markers" only need replay delivery. A new
# decoder must declare its lines here so both are pinned instead of
# discovered in the field.
CRASH_SAMPLES: dict[str, dict[str, list[str]]] = {
    "esp32": {
        "state_markers": [],
        "context_markers": [],
        "addresses": [
            "Backtrace: 0x400d1a2c:0x3ffb1f60 0x400d2a3c:0x3ffb1f80",
            "PC      : 0x400d1a2c  PS      : 0x00060330",
            "EXCVADDR: 0x40001234",
            "MEPC    : 0x40380abc  RA      : 0x40380def",
            "MTVAL   : 0x40000123",
            "last failed alloc call: 40201234(512)",
            "BT0: 0x40104960",
        ],
    },
    "esp8266": {
        "state_markers": [">>>stack>>>"],
        "context_markers": ["Exception (28):"],
        "addresses": [
            "epc1=0x40201234 epc2=0x00000000 excvaddr=0x40001234",
            "3ffffe10: 40201234 3ffe8410 00000000 40201000",
            "PC      : 40201234",
            "EXCVADDR: 0x40001234",
            "BT0: 0x40201234",
            "last failed alloc call: 40201234(512)",
        ],
    },
    "rp2": {
        "state_markers": ["CRASH DETECTED ON PREVIOUS BOOT"],
        "context_markers": [],
        "addresses": ["PC:  0x10001234 (fault location)"],
    },
    "nrf52": {
        "state_markers": ["Last crash:"],
        "context_markers": [],
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
    pytest.param("[I][app:102]: Uptime: 41234567 ms", id="decimal-uptime-4band"),
    pytest.param("[V][esp-idf:000]: I (40219876) wifi: connected", id="idf-timestamp"),
    pytest.param("[D][api:102]: Client connected (40123456)", id="paren-decimal"),
    pytest.param(
        "[D][sensor:093]: 'Water meter': Sending state 12345678.00000 L",
        id="decimal-sensor",
    ),
]


def test_crash_samples_cover_registry() -> None:
    """A newly registered decoder must come with a non-empty gate sample."""
    from esphome.platform_hooks import PLATFORM_HOOKS

    assert set(CRASH_SAMPLES) == set(PLATFORM_HOOKS["process_stacktrace"])
    assert all(samples["addresses"] for samples in CRASH_SAMPLES.values())


# Derived, not hand-listed: a new decoder cannot declare a state marker
# without the gate test below having to pass on it.
STATE_MARKERS = [
    marker for samples in CRASH_SAMPLES.values() for marker in samples["state_markers"]
]


@pytest.mark.parametrize("line", CRASH_LINES + STATE_MARKERS)
def test_address_gate_fires_on_platform_dump_lines(line: str) -> None:
    assert stacktrace._ADDRESS_RE.search(line)


@pytest.mark.parametrize("platform", sorted(CRASH_SAMPLES))
def test_markers_reach_the_decoder_before_addresses(platform: str) -> None:
    """The replay buffer delivers each platform's markers in order.

    For state-gated decoders the marker is load-bearing: without it the
    address lines decode to nothing. This fails if a marker outgrows the
    replay window or the delivery order breaks.
    """
    markers = (
        CRASH_SAMPLES[platform]["context_markers"]
        + CRASH_SAMPLES[platform]["state_markers"]
    )
    addresses = CRASH_SAMPLES[platform]["addresses"]
    handler = Mock(return_value=True)

    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, platform)
        for line in markers + addresses:
            processor.process_line(line)

    assert [call.args[1] for call in handler.call_args_list] == markers + addresses


# The stacktrace pattern constants each decoder module exports. The
# samples and these patterns must cover each other, so an edit on either
# side fails the test below instead of quietly widening the gap between
# the gate and the decoders.
DECODER_PATTERNS: dict[str, list[str]] = {
    "esp32": [
        "STACKTRACE_ESP32_PC_RE",
        "STACKTRACE_ESP32_EXCVADDR_RE",
        "STACKTRACE_ESP32_C3_PC_RE",
        "STACKTRACE_ESP32_C3_RA_RE",
        "STACKTRACE_ESP32_C3_MTVAL_RE",
        "STACKTRACE_BAD_ALLOC_RE",
        "STACKTRACE_ESP32_BACKTRACE_RE",
        "STACKTRACE_ESP32_BACKTRACE_PC_RE",
        "STACKTRACE_ESP32_CRASH_BT_RE",
    ],
    "esp8266": [
        "STACKTRACE_ESP8266_EXCEPTION_TYPE_RE",
        "STACKTRACE_ESP8266_PC_RE",
        "STACKTRACE_ESP8266_EXCVADDR_RE",
        "STACKTRACE_ESP8266_CRASH_PC_RE",
        "STACKTRACE_ESP8266_CRASH_EXCVADDR_RE",
        "STACKTRACE_ESP8266_CRASH_BT_RE",
        "STACKTRACE_BAD_ALLOC_RE",
        "STACKTRACE_ESP8266_BACKTRACE_PC_RE",
    ],
    "rp2": ["_CRASH_RE", "_CRASH_ADDR_RE"],
    "nrf52": ["STACKTRACE_NRF52_PC_LR_RE"],
}


@pytest.mark.parametrize("platform", sorted(CRASH_SAMPLES))
def test_samples_and_decoder_patterns_cover_each_other(platform: str) -> None:
    """Samples stay mechanically linked to the decoder regexes.

    Three directions: every declared pattern must exist, every address
    sample must match a declared pattern, and every declared pattern
    must be exercised by a sample - so a decoder gaining a new address
    form cannot ship without a sample the gate test then has to pass.

    Known blind spots: the esp32/esp8266 catch-all backtrace patterns
    can satisfy the sample-matches-a-pattern direction on their own, and
    the undeclared-pattern sweep keys off naming, so a differently-named
    constant or a function-local re.search literal is invisible to it.
    """
    import importlib
    import re

    module = importlib.import_module(f"esphome.components.{platform}")
    patterns: dict[str, re.Pattern] = {}
    for name in DECODER_PATTERNS[platform]:
        pattern = getattr(module, name, None)
        if pattern is None:
            pytest.fail(
                f"{platform} no longer defines {name}; update DECODER_PATTERNS "
                "and CRASH_SAMPLES together"
            )
        patterns[name] = pattern

    lines = (
        CRASH_SAMPLES[platform]["state_markers"]
        + CRASH_SAMPLES[platform]["context_markers"]
        + CRASH_SAMPLES[platform]["addresses"]
    )
    for line in CRASH_SAMPLES[platform]["addresses"]:
        assert any(p.search(line) for p in patterns.values()), (
            f"{line!r} no longer matches any {platform} decoder pattern; "
            "update CRASH_SAMPLES and re-derive the gate"
        )
    for name, pattern in patterns.items():
        assert any(pattern.search(line) for line in lines), (
            f"no sample exercises {platform}.{name}; add one so the gate "
            "provably covers it"
        )

    undeclared = [
        name
        for name, value in vars(module).items()
        if isinstance(value, re.Pattern)
        and ("STACKTRACE" in name or name.startswith("_CRASH"))
        and name not in DECODER_PATTERNS[platform]
    ]
    assert not undeclared, (
        f"{platform} gained stacktrace patterns {undeclared}; declare them in "
        "DECODER_PATTERNS with samples"
    )


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


@pytest.mark.parametrize(("quiet_lines", "expect_notice"), [(12, True), (8, False)])
def test_replay_overflow_is_diagnosable(
    caplog, quiet_lines: int, expect_notice: bool
) -> None:
    """The overflow trace fires only when context was actually evicted."""
    caplog.set_level("DEBUG", logger="esphome.stacktrace")
    handler = Mock(return_value=False)
    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ):
        processor = stacktrace.LogLineProcessor(CONFIG, "esp32")
        for n in range(quiet_lines):
            processor.process_line(f"quiet line {n}")
        processor.process_line("PC: 0x4010496e")

    assert ("Replay window held only" in caplog.text) is expect_notice


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
    # The cause rides along at default verbosity, like the sibling paths.
    assert "could not be loaded: filesystem went away" in warnings[0].message
    assert processor.backtrace_state is False


def test_decoder_bug_with_empty_message_names_the_type(caplog) -> None:
    """A zero-message decoder bug must not masquerade as missing artifacts.

    The recompile hint is only right for EsphomeError from _run_idedata;
    anything else would send the user down a dead-end remediation path.
    """
    _make_processor(Mock(side_effect=IndexError()))

    warnings = [r.message for r in caplog.records if r.levelname == "WARNING"]
    assert any("IndexError" in m for m in warnings)
    assert not any("build artifacts not found locally" in m for m in warnings)


def test_external_platform_resolves_at_construction(caplog) -> None:
    """External platforms resolve eagerly, like before the registry.

    The address gate's grammar derives from the in-tree decoders, so it
    cannot speak for an external decoder; resolving up front keeps the
    import off the event loop and the notice at session start.
    """
    caplog.set_level("INFO", logger="esphome.platform_hooks")
    module = type("ExternalPlatform", (), {})  # no process_stacktrace

    with patch.object(
        stacktrace.platform_hooks,
        "import_module",
        Mock(return_value=module),
    ) as mock_import:
        processor = stacktrace.LogLineProcessor(CONFIG, "my_external_chip")
        mock_import.assert_called_once()
        assert "Stacktrace analysis is unavailable" in caplog.text

        processor.process_line("PC: 0x40104960")

    mock_import.assert_called_once()
    assert processor.backtrace_state is False


def test_processor_import_failure_disables_decoding(caplog) -> None:
    """A broken platform package degrades once instead of raising."""
    caplog.set_level("INFO", logger="esphome.platform_hooks")

    with patch.object(
        stacktrace.platform_hooks,
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

    with patch.object(stacktrace.platform_hooks, "import_module") as mock_import:
        processor = stacktrace.LogLineProcessor(CONFIG, "bk72xx")
        processor.process_line("PC: 0x40104960")

    mock_import.assert_not_called()
    assert "Stacktrace analysis is unavailable" in caplog.text
    assert processor.backtrace_state is False
