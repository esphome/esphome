"""Tests for esphome.stacktrace."""

from __future__ import annotations

import importlib
import inspect
import re
from unittest.mock import Mock, patch

from hypothesis import given, settings
from hypothesis.strategies import data as st_data, from_regex
import pytest

from esphome import stacktrace
from esphome.const import (
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_NRF52,
    PLATFORM_RP2,
)
from esphome.core import EsphomeError

CONFIG = {"esphome": {"name": "test"}}

# Real dump lines per registered platform. "addresses" are gate-firing
# dump lines (registers, backtraces, the exception header); the gate
# must fire on each or that platform's decoding silently never starts.
# "state_markers" are the address-free lines that set backtrace_state;
# the gate matches them directly so their decoders never miss the line
# that opens their dump region. A new decoder must declare its lines
# here so both are pinned instead of discovered in the field.
CRASH_SAMPLES: dict[str, dict[str, list[str]]] = {
    PLATFORM_ESP32: {
        "state_markers": [],
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
    PLATFORM_ESP8266: {
        "state_markers": [">>>stack>>>"],
        "addresses": [
            "epc1=0x40201234 epc2=0x00000000 excvaddr=0x40001234",
            "3ffffe10: 40201234 3ffe8410 00000000 40201000",
            "PC      : 40201234",
            "EXCVADDR: 0x40001234",
            "BT0: 0x40201234",
            "last failed alloc call: 40201234(512)",
            "Exception (28):",
        ],
    },
    PLATFORM_RP2: {
        "state_markers": ["CRASH DETECTED ON PREVIOUS BOOT"],
        "addresses": ["PC:  0x10001234 (fault location)"],
    },
    PLATFORM_NRF52: {
        "state_markers": ["Last crash:"],
        "addresses": ["PC=0x27a1c LR=0x1e33"],
    },
}

BENIGN_LINES = [
    "[I][app:100] hello world",
    "[C][wifi:400]   BSSID: AA:BB:CC:DD:EE:FF",
    "[19:26:11.966][I][main:151]: version 2026.7.0-dev",
    "[I][app:102]: Uptime: 12345678 ms",
    "[I][app:102]: Uptime: 41234567 ms",
    "[V][esp-idf:000]: I (40219876) wifi: connected",
    "[D][api:102]: Client connected (40123456)",
    "[D][sensor:093]: 'Water meter': Sending state 12345678.00000 L",
]

GATE_PARAMS = [
    pytest.param(line, True, id=f"{platform}-{kind}-{n}")
    for platform, samples in CRASH_SAMPLES.items()
    for kind in ("addresses", "state_markers")
    for n, line in enumerate(samples[kind])
] + [pytest.param(line, False, id=f"benign-{n}") for n, line in enumerate(BENIGN_LINES)]


@pytest.mark.parametrize(("line", "should_fire"), GATE_PARAMS)
def test_address_gate(line: str, should_fire: bool) -> None:
    assert bool(stacktrace._ADDRESS_RE.search(line)) is should_fire


def test_crash_samples_cover_registry() -> None:
    """A newly registered decoder must come with a non-empty gate sample."""
    assert set(CRASH_SAMPLES) == set(
        stacktrace.platform_hooks.PLATFORM_HOOKS["process_stacktrace"]
    )
    assert all(samples["addresses"] for samples in CRASH_SAMPLES.values())


# The stacktrace pattern constants each decoder module exports. The
# samples and these patterns must cover each other, so an edit on either
# side fails the guards below instead of quietly widening the gap
# between the gate and the decoders.
DECODER_PATTERNS: dict[str, list[str]] = {
    PLATFORM_ESP32: [
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
    PLATFORM_ESP8266: [
        "STACKTRACE_ESP8266_EXCEPTION_TYPE_RE",
        "STACKTRACE_ESP8266_PC_RE",
        "STACKTRACE_ESP8266_EXCVADDR_RE",
        "STACKTRACE_ESP8266_CRASH_PC_RE",
        "STACKTRACE_ESP8266_CRASH_EXCVADDR_RE",
        "STACKTRACE_ESP8266_CRASH_BT_RE",
        "STACKTRACE_BAD_ALLOC_RE",
        "STACKTRACE_ESP8266_BACKTRACE_PC_RE",
    ],
    PLATFORM_RP2: ["_CRASH_RE", "_CRASH_ADDR_RE"],
    PLATFORM_NRF52: ["STACKTRACE_NRF52_PC_LR_RE"],
}

# Declared decoder patterns whose language the gate deliberately does
# not cover: bare stack-dump words, where the gate keys on the dump
# line's 3ff... stack address instead and a lone letter-free word never
# appears outside a dump region whose other lines already fired.
GATE_EXEMPT_PATTERNS = {
    "STACKTRACE_ESP32_BACKTRACE_PC_RE",
    "STACKTRACE_ESP8266_BACKTRACE_PC_RE",
}


@pytest.mark.parametrize("platform", sorted(CRASH_SAMPLES))
def test_platform_declarations_match_decoder(platform: str) -> None:
    """Samples, declared patterns, and the decoder must agree.

    Directions checked: every declared pattern exists; every address
    sample matches a declared pattern; every declared pattern is
    exercised by a sample; no stacktrace pattern exists undeclared; each
    declared state marker behaviourally opens the decoder's dump region;
    and a decoder that sets state must declare a marker while a
    stateless one must not.

    Known blind spots: the esp32/esp8266 catch-all backtrace patterns
    can satisfy the sample-matches-a-pattern direction on their own; the
    undeclared-pattern sweep keys off naming, so a differently-named
    constant or a function-local re.search literal is invisible to it;
    the state-gated detection rests on a textual heuristic (every
    such decoder today spells it as ``return True`` or
    ``backtrace_state = True``); and a decoder that gains a second
    opening marker alongside a declared one passes unnoticed, since
    the declared marker already satisfies both the marker-opens-region
    check and the non-empty ``state_markers`` requirement.
    """
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
        CRASH_SAMPLES[platform]["state_markers"] + CRASH_SAMPLES[platform]["addresses"]
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

    for marker in CRASH_SAMPLES[platform]["state_markers"]:
        assert module.process_stacktrace(CONFIG, marker, False) is True, (
            f"{marker!r} no longer opens {platform}'s dump region; update "
            "state_markers to the line the decoder actually keys on"
        )
    # Textual heuristic, deliberately one-directional: a state-gated
    # decoder must declare a marker. The reverse (a stateless decoder
    # declaring none) is not asserted; an unrelated "return True" added
    # to a decoder would turn it into a false failure.
    source = inspect.getsource(module.process_stacktrace)
    sets_state = "return True" in source or "backtrace_state = True" in source
    if sets_state:
        assert CRASH_SAMPLES[platform]["state_markers"], (
            f"{platform}.process_stacktrace is state-gated but declares no "
            "state_markers; the gate cannot promise to open its dump region"
        )


@pytest.mark.parametrize(
    ("platform", "name"),
    [
        (platform, name)
        for platform, names in DECODER_PATTERNS.items()
        for name in names
        if name not in GATE_EXEMPT_PATTERNS
    ],
)
@given(data=st_data())
@settings(max_examples=25, deadline=None)
def test_address_gate_covers_decoder_pattern_languages(
    platform: str, name: str, data
) -> None:
    """The gate must be a superset of each decoder pattern's language.

    The sample table only pins finite literals; a decoder regex that
    widens would keep every sample green while the gate misses the new
    form. Generating inputs from the decoder regex itself closes that
    direction.
    """
    pattern = getattr(importlib.import_module(f"esphome.components.{platform}"), name)
    example = data.draw(from_regex(pattern, fullmatch=True))
    assert stacktrace._ADDRESS_RE.search(example), (
        f"{platform}.{name} accepts {example!r} but the gate does not fire; "
        "decoding would silently never start on that form"
    )


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
    with patch.object(
        stacktrace.time, "monotonic", side_effect=[0.0, 30.0, 61.0, 61.0]
    ):
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
    with patch.object(stacktrace.time, "monotonic", side_effect=[0.0, 61.0, 61.0]):
        _run(handler, lines=("PC: 0x4010496e", "BT0: 0x4010496e"))

    assert handler.call_count == 2
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
    with patch.object(stacktrace.time, "monotonic", return_value=1e9):
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
    """_run_idedata raises EsphomeError with no message; the warning
    must show a useful explanation rather than empty parens.
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


def test_marker_then_address_threads_state() -> None:
    """A state marker resolves the decoder and threads state onward.

    esp8266's ``>>>stack>>>`` fires the gate itself, so the decoder sees
    it live and the following stack words decode inside the region.
    """
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


def test_lines_before_the_gate_never_reach_the_decoder() -> None:
    """Benign lines are dropped, not buffered: the gate is a superset of
    the decoder languages, so a line that fails it cannot decode.
    """
    handler = Mock(return_value=False)
    quiet = tuple(f"quiet line {n}" for n in range(12))
    _run(handler, lines=quiet + ("PC: 0x4010496e",))

    assert _fed(handler) == ["PC: 0x4010496e"]


def test_processor_resolves_lazily_on_address_token() -> None:
    """No resolution attempt until a line carries an address token."""
    handler = Mock(return_value=False)

    with patch.object(
        stacktrace.platform_hooks, "get_stacktrace_handler", return_value=handler
    ) as mock_resolve:
        processor = stacktrace.LogLineProcessor(CONFIG, PLATFORM_ESP32)
        processor.process_line("[I][app:100] hello world")
        mock_resolve.assert_not_called()

        processor.process_line("PC: 0x40104960")
        mock_resolve.assert_called_once_with(PLATFORM_ESP32)

        # Later lines feed the resolved handler directly, no re-resolution.
        processor.process_line("[I][app:101] back to normal")
        mock_resolve.assert_called_once()

    assert _fed(handler) == ["PC: 0x40104960", "[I][app:101] back to normal"]


def test_processor_unexpected_resolution_error_disables_decoding(caplog) -> None:
    """Resolution is inside the containment guarantee like everything else."""
    with patch.object(
        stacktrace.platform_hooks,
        "get_stacktrace_handler",
        side_effect=OSError("filesystem went away"),
    ) as mock_resolve:
        processor = stacktrace.LogLineProcessor(CONFIG, PLATFORM_ESP32)
        processor.process_line("PC: 0x40104960")
        processor.process_line("BT0: 0x40104960")

    mock_resolve.assert_called_once()
    warnings = _warnings(caplog)
    assert len(warnings) == 1
    assert "could not be loaded" in warnings[0]
    assert processor.backtrace_state is False


def test_processor_import_failure_disables_decoding(caplog) -> None:
    """A broken platform package degrades once instead of raising."""
    caplog.set_level("INFO", logger="esphome.platform_hooks")

    with patch.object(
        stacktrace.platform_hooks,
        "import_module",
        Mock(side_effect=ImportError("broken install")),
    ) as mock_import:
        processor = stacktrace.LogLineProcessor(CONFIG, PLATFORM_ESP32)
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
        processor = stacktrace.LogLineProcessor(CONFIG, PLATFORM_BK72XX)
        processor.process_line("PC: 0x40104960")

    mock_import.assert_not_called()
    assert "Stacktrace analysis is unavailable" in caplog.text
    assert processor.backtrace_state is False


def test_external_platform_resolves_at_construction(caplog) -> None:
    """External platforms resolve eagerly, like before the registry.

    The address gate's grammar derives from the in-tree decoders, so it
    cannot speak for an external decoder; resolving up front keeps the
    import off the streaming callback and the notice at session start.
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
