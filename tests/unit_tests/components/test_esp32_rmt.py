"""Tests for the ESP32 RMT channel and memory budget check."""

from collections.abc import Callable
import logging
from typing import Any

import pytest

from esphome.components import esp32_rmt
from esphome.components.esp32 import (
    KEY_ESP32,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32H4,
    VARIANT_ESP32H21,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_RMT_SYMBOLS,
    CONF_USE_DMA,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE, ID

# Entries of RMT components, by direction
Config = dict[str, list[dict[str, Any]]]

# Runs the check for a variant against the entries, returns the RMT warnings
CheckRunner = Callable[..., list[str]]


def _entry(name: str, symbols: int, **extra: object) -> dict[str, Any]:
    return {CONF_ID: ID(name, is_manual=True), CONF_RMT_SYMBOLS: symbols, **extra}


def _strips(*symbols: int) -> Config:
    return {"TX": [_entry(f"strip{i}", value) for i, value in enumerate(symbols)]}


def _set_variant(variant: str, platform: str = PLATFORM_ESP32) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
    CORE.data[KEY_ESP32] = {KEY_VARIANT: variant}
    CORE.data.pop(esp32_rmt.DOMAIN, None)


def _run(config: Config) -> None:
    """Validate the entries like ESPHome does: all schemas first, then final validation."""
    for direction, entries in config.items():
        for entry in entries:
            esp32_rmt.request_rmt_channel(direction)(entry)
    for entries in config.values():
        for entry in entries:
            esp32_rmt.final_validate_rmt_channels(entry)


@pytest.fixture
def run_check(caplog: pytest.LogCaptureFixture) -> CheckRunner:
    def run(variant: str, config: Config, platform: str = PLATFORM_ESP32) -> list[str]:
        _set_variant(variant, platform)
        caplog.clear()
        with caplog.at_level(logging.WARNING):
            _run(config)
        return [r.getMessage() for r in caplog.records if "RMT" in r.getMessage()]

    return run


@pytest.mark.parametrize(
    ("variant", "symbols", "warns"),
    [
        # ESP32: 8 shared blocks of 64, so two strips at the default 192 fit and three do not
        (VARIANT_ESP32, (192, 192), False),
        (VARIANT_ESP32, (192, 192, 192), True),
        # ESP32-S2: 4 shared blocks of 64
        (VARIANT_ESP32S2, (192,), False),
        (VARIANT_ESP32S2, (192, 192), True),
        (VARIANT_ESP32S2, (128, 128), False),
        (VARIANT_ESP32S2, (64, 64), False),
        # ESP32-S3 and P4: 4 TX blocks of 48
        (VARIANT_ESP32S3, (192,), False),
        (VARIANT_ESP32S3, (192, 192), True),
        (VARIANT_ESP32S3, (96, 96), False),
        (VARIANT_ESP32P4, (192, 192), True),
        (VARIANT_ESP32P4, (48, 48), False),
        (VARIANT_ESP32S31, (192, 192), True),
        (VARIANT_ESP32S31, (96, 96), False),
        # C3, C5, C6, H2, H4, H21: 2 TX blocks of 48, the default is 96
        (VARIANT_ESP32C3, (96,), False),
        (VARIANT_ESP32C3, (96, 96), True),
        (VARIANT_ESP32C5, (96, 96), True),
        (VARIANT_ESP32C6, (96, 96), True),
        (VARIANT_ESP32C6, (48, 48), False),
        (VARIANT_ESP32H2, (48, 48), False),
        (VARIANT_ESP32H4, (96, 96), True),
        (VARIANT_ESP32H21, (48, 48), False),
        # The driver allocates whole blocks: 50 symbols on S3 take two blocks of 48
        (VARIANT_ESP32S3, (50, 50), False),
        (VARIANT_ESP32S3, (50, 50, 48), True),
    ],
)
def test_strip_memory(
    run_check: CheckRunner, variant: str, symbols: tuple[int, ...], warns: bool
) -> None:
    """Strips are counted in whole blocks against the variant's TX memory."""
    assert bool(run_check(variant, _strips(*symbols))) is warns


def test_warning_names_components_and_suggests_value(run_check: CheckRunner) -> None:
    """Internal status LEDs and an external NeoPixel port at the default 192 on S2."""
    lights = [
        {**_entry("status_leds", 192), CONF_NAME: "Status LEDs"},
        {**_entry("neopixel_port", 192), CONF_NAME: "NeoPixel Port"},
    ]
    messages = run_check(VARIANT_ESP32S2, {"TX": lights})
    assert len(messages) == 1
    assert "holds 4 blocks of 64 symbols" in messages[0]
    assert "need 6 blocks: 'Status LEDs' (192), 'NeoPixel Port' (192)" in messages[0]
    assert "Set 'rmt_symbols: 128' on 'Status LEDs', 'NeoPixel Port'." in messages[0]


def test_suggestion_skips_components_that_already_fit(run_check: CheckRunner) -> None:
    """A transmitter below the suggested value is not told to grow."""
    config = {
        "TX": [*_strips(192, 192)["TX"], _entry("ir_tx", 64)],
        "RX": [_entry("ir_rx", 192)],
    }
    messages = run_check(VARIANT_ESP32, config)
    assert len(messages) == 1
    assert "Set 'rmt_symbols: 128' on 'strip0', 'strip1', 'ir_rx'." in messages[0]


def test_strip_and_transmitter_share_tx(run_check: CheckRunner) -> None:
    """remote_transmitter and esp32_rmt_led_strip share the TX blocks (esphome#17894)."""
    config = {"TX": [*_strips(192)["TX"], _entry("ir", 48)]}
    messages = run_check(VARIANT_ESP32S3, config)
    assert len(messages) == 1
    assert "RMT TX memory" in messages[0]
    assert "ESP-IDF logs 'no free tx channels'," in messages[0]


def test_tx_warning_names_rx_error_with_receivers(run_check: CheckRunner) -> None:
    """A TX channel that overruns can take RX blocks, so a receiver can be the one that fails."""
    config = {**_strips(192, 96), "RX": [_entry("rx", 48)]}
    messages = run_check(VARIANT_ESP32S3, config)
    assert len(messages) == 1
    assert "'no free tx channels' or 'no free rx channels'" in messages[0]


def test_receiver_uses_own_rx_blocks(run_check: CheckRunner) -> None:
    """On S3 the receiver has its own RX blocks and does not take TX memory."""
    config = {**_strips(192), "RX": [_entry("rx", 192)]}
    assert not run_check(VARIANT_ESP32S3, config)


@pytest.mark.parametrize("with_transmitter", [False, True])
def test_receiver_shares_blocks_on_s2(
    run_check: CheckRunner, with_transmitter: bool
) -> None:
    """On S2 a 64 symbol strip and a 192 symbol receiver fill the four shared blocks."""
    config = {**_strips(64), "RX": [_entry("rx", 192)]}
    if with_transmitter:
        config["TX"].append(_entry("ir", 64))
    messages = run_check(VARIANT_ESP32S2, config)
    assert len(messages) == with_transmitter
    if with_transmitter:
        assert "RMT TX/RX memory" in messages[0]


def test_dma_channel_uses_one_block(run_check: CheckRunner) -> None:
    """A DMA channel takes one block whatever its rmt_symbols, which size the DMA buffer."""
    lights = [_entry("dma", 1024, **{CONF_USE_DMA: True}), _entry("other", 144)]
    assert not run_check(VARIANT_ESP32S3, {"TX": lights})


def test_suggestion_leaves_out_dma_channels(run_check: CheckRunner) -> None:
    """The suggested value splits only the blocks the DMA channel leaves free."""
    lights = [_entry("dma", 192, **{CONF_USE_DMA: True}), _entry("other", 192)]
    messages = run_check(VARIANT_ESP32S3, {"TX": lights})
    assert len(messages) == 1
    assert "'dma' (DMA), 'other' (192)" in messages[0]
    assert "Set 'rmt_symbols: 144' on 'other'." in messages[0]


@pytest.mark.parametrize(
    ("variant", "direction", "symbols", "extra", "match"),
    [
        # The original ESP32 has blocks of 64 symbols
        (VARIANT_ESP32, "TX", 48, {}, "even number and at least 64 on ESP32"),
        (VARIANT_ESP32S3, "RX", 97, {}, "even number and at least 48 on ESP32S3"),
        # Also with DMA, where rmt_symbols sizes the DMA buffer
        (VARIANT_ESP32S3, "TX", 40, {CONF_USE_DMA: True}, "at least 48"),
        (VARIANT_ESP32S3, "TX", 2048, {CONF_USE_DMA: True}, "at most 2046 with"),
        # One channel cannot use more memory than there is
        (VARIANT_ESP32S2, "TX", 320, {}, "at most 256 on ESP32S2"),
        (VARIANT_ESP32C3, "RX", 144, {}, "at most 96 on ESP32C3"),
        (VARIANT_ESP32C3, "TX", 240, {}, "at most 192 on ESP32C3"),
    ],
)
def test_invalid_rmt_symbols_fail(
    variant: str,
    direction: esp32_rmt.Direction,
    symbols: int,
    extra: dict[str, object],
    match: str,
) -> None:
    """The driver refuses these values when it creates the channel."""
    _set_variant(variant)
    with pytest.raises(cv.Invalid, match=match) as error:
        esp32_rmt.request_rmt_channel(direction)(_entry("x", symbols, **extra))
    assert error.value.path == [CONF_RMT_SYMBOLS]


@pytest.mark.parametrize(
    ("direction", "symbols", "extra"),
    [
        # A TX channel can extend into the RX blocks: 384 on S3 boots
        ("TX", 384, {}),
        ("RX", 192, {}),
        # RX has no DMA buffer limit
        ("RX", 4096, {CONF_USE_DMA: True}),
        ("TX", 2046, {CONF_USE_DMA: True}),
    ],
)
def test_valid_rmt_symbols_pass(
    direction: esp32_rmt.Direction, symbols: int, extra: dict[str, object]
) -> None:
    _set_variant(VARIANT_ESP32S3)
    entry = _entry("x", symbols, **extra)
    assert esp32_rmt.request_rmt_channel(direction)(entry) is entry


def test_two_dma_channels_in_one_direction_fail(run_check: CheckRunner) -> None:
    """Only the last channel of each direction supports DMA."""
    config = {
        "TX": [
            _entry("dma", 48, **{CONF_USE_DMA: True}),
            _entry("ir", 48, **{CONF_USE_DMA: True}),
        ]
    }
    with pytest.raises(cv.Invalid, match="Only one RMT TX channel on ESP32S3") as error:
        run_check(VARIANT_ESP32S3, config)
    assert error.value.path == [CONF_USE_DMA]


def test_dma_on_tx_and_rx(run_check: CheckRunner) -> None:
    """One DMA channel per direction is fine."""
    config = {
        "TX": [_entry("dma", 48, **{CONF_USE_DMA: True})],
        "RX": [_entry("rx", 48, **{CONF_USE_DMA: True})],
    }
    assert not run_check(VARIANT_ESP32S3, config)


@pytest.mark.parametrize(
    ("variant", "config", "match"),
    [
        (VARIANT_ESP32C3, _strips(48, 48, 48), "ESP32C3 has 2 RMT TX channels"),
        (
            VARIANT_ESP32S2,
            {**_strips(64, 64, 64, 64), "RX": [_entry("rx", 64)]},
            "ESP32S2 has 4 RMT TX/RX channels, but these 5 components",
        ),
        (
            VARIANT_ESP32S3,
            {"RX": [_entry(f"rx{i}", 48) for i in range(5)]},
            "ESP32S3 has 4 RMT RX channels",
        ),
    ],
)
def test_more_components_than_channels_fail(
    run_check: CheckRunner, variant: str, config: Config, match: str
) -> None:
    """Every component needs its own channel, whatever its rmt_symbols."""
    with pytest.raises(cv.Invalid, match=match):
        run_check(variant, config)


def test_error_raised_from_a_component_that_causes_it() -> None:
    """The error is not raised from an RMT component that is not involved."""
    _set_variant(VARIANT_ESP32S3)
    receiver = _entry("rx", 48)
    transmitters = [_entry(f"tx{i}", 48) for i in range(5)]
    receivers = [
        receiver,
        *(_entry(f"rx{i}", 48, **{CONF_USE_DMA: True}) for i in range(2)),
    ]
    for entry in transmitters:
        esp32_rmt.request_rmt_channel("TX")(entry)
    for entry in receivers:
        esp32_rmt.request_rmt_channel("RX")(entry)
    esp32_rmt.final_validate_rmt_channels(receiver)
    with pytest.raises(cv.Invalid) as error:
        esp32_rmt.final_validate_rmt_channels(transmitters[0])
    # Both errors are reported at once, as final validation stops after the first one
    assert "ESP32S3 has 4 RMT TX channels" in str(error.value)
    assert "Only one RMT RX channel on ESP32S3 supports DMA" in str(error.value)


@pytest.mark.parametrize(
    ("platform", "variant"),
    [
        (PLATFORM_ESP8266, VARIANT_ESP32S3),
        (PLATFORM_ESP32, VARIANT_ESP32C2),
        (PLATFORM_ESP32, VARIANT_ESP32C61),
        (PLATFORM_ESP32, "ESP32X1"),
    ],
)
def test_not_checked_without_rmt_caps(
    run_check: CheckRunner, platform: str, variant: str
) -> None:
    """Other platforms, variants without RMT and variants not in the table are skipped."""
    assert not run_check(variant, _strips(192, 192, 192, 192, 192, 7), platform)


def test_not_checked_in_testing_mode(
    run_check: CheckRunner, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Grouped component tests merge many configs, so the combined check is skipped there."""
    monkeypatch.setattr(CORE, "testing_mode", True)
    assert not run_check(VARIANT_ESP32C3, _strips(96, 96, 96))


def test_check_runs_once(
    run_check: CheckRunner, caplog: pytest.LogCaptureFixture
) -> None:
    """Every RMT component calls the check; it warns once per config."""
    run_check(VARIANT_ESP32S3, _strips(192, 192))
    caplog.clear()
    with caplog.at_level(logging.WARNING):
        esp32_rmt.final_validate_rmt_channels({})
    assert not caplog.records
