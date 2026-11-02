"""Tests for the ESP32 RMT channel and memory budget check."""

from collections.abc import Callable, Generator
import contextvars
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
    VARIANT_ESP32H2,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_PLATFORM,
    CONF_RMT_SYMBOLS,
    CONF_USE_DMA,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE, ID
import esphome.final_validate as fv

# A full config as final validation sees it
Config = dict[str, Any]

# Runs the check for a variant against a full config, returns the RMT warnings
CheckRunner = Callable[..., list[str]]


def _strip(name: str, symbols: int, **extra: object) -> Config:
    return {
        CONF_PLATFORM: "esp32_rmt_led_strip",
        CONF_ID: ID(name, is_manual=True),
        CONF_RMT_SYMBOLS: symbols,
        **extra,
    }


def _remote(name: str, symbols: int, **extra: object) -> Config:
    return {CONF_ID: ID(name, is_manual=True), CONF_RMT_SYMBOLS: symbols, **extra}


def _strips(*symbols: int) -> Config:
    return {"light": [_strip(f"strip{i}", value) for i, value in enumerate(symbols)]}


@pytest.fixture
def run_check(caplog: pytest.LogCaptureFixture) -> Generator[CheckRunner]:
    tokens: list[contextvars.Token] = []

    def run(variant: str, config: Config, platform: str = PLATFORM_ESP32) -> list[str]:
        CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
        CORE.data[KEY_ESP32] = {KEY_VARIANT: variant}
        CORE.data.pop(esp32_rmt.DOMAIN, None)
        tokens.append(fv.full_config.set(config))
        caplog.clear()
        with caplog.at_level(logging.WARNING):
            # Final validation calls the check once for each RMT component, in this order
            for domain in ("light", "remote_transmitter", "remote_receiver"):
                for entry in config.get(domain, []):
                    esp32_rmt.validate_symbol_budget(entry)
        return [r.getMessage() for r in caplog.records if "RMT" in r.getMessage()]

    yield run
    for token in reversed(tokens):
        fv.full_config.reset(token)


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
        # C3, C5, C6, H2: 2 TX blocks of 48, the default is 96
        (VARIANT_ESP32C3, (96,), False),
        (VARIANT_ESP32C3, (96, 96), True),
        (VARIANT_ESP32C5, (96, 96), True),
        (VARIANT_ESP32C6, (96, 96), True),
        (VARIANT_ESP32C6, (48, 48), False),
        (VARIANT_ESP32H2, (48, 48), False),
        # The driver allocates whole blocks: 49 symbols on S3 take two blocks of 48
        (VARIANT_ESP32S3, (49, 49), False),
        (VARIANT_ESP32S3, (49, 49, 48), True),
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
        {**_strip("status_leds", 192), CONF_NAME: "Status LEDs"},
        {**_strip("neopixel_port", 192), CONF_NAME: "NeoPixel Port"},
    ]
    messages = run_check(VARIANT_ESP32S2, {"light": lights})
    assert len(messages) == 1
    assert "holds 4 blocks of 64 symbols" in messages[0]
    assert "need 6 blocks: 'Status LEDs' (192), 'NeoPixel Port' (192)" in messages[0]
    assert "Set 'rmt_symbols: 128' on 'Status LEDs', 'NeoPixel Port'." in messages[0]


def test_suggestion_skips_components_that_already_fit(run_check: CheckRunner) -> None:
    """A transmitter below the suggested value is not told to grow."""
    config = {
        **_strips(192, 192),
        "remote_transmitter": [_remote("ir_tx", 64)],
        "remote_receiver": [_remote("ir_rx", 192)],
    }
    messages = run_check(VARIANT_ESP32, config)
    assert len(messages) == 1
    assert "Set 'rmt_symbols: 128' on 'strip0', 'strip1', 'ir_rx'." in messages[0]


def test_unnamed_components_use_domain_index(run_check: CheckRunner) -> None:
    """Components without a name or manual id are named by their position."""
    config = {
        "light": [{**_strip("x", 192), CONF_ID: ID("auto", is_manual=False)}],
        "remote_transmitter": [
            {**_remote("y", 48), CONF_ID: ID(f"auto{i}", is_manual=False)}
            for i in range(2)
        ],
    }
    messages = run_check(VARIANT_ESP32S3, config)
    assert (
        "'light[0]' (192), 'remote_transmitter[0]' (48), 'remote_transmitter[1]' (48)"
        in messages[0]
    )


def test_strip_and_transmitter_share_tx(run_check: CheckRunner) -> None:
    """remote_transmitter and esp32_rmt_led_strip share the TX blocks (esphome#17894)."""
    config = {**_strips(192), "remote_transmitter": [_remote("ir", 48)]}
    messages = run_check(VARIANT_ESP32S3, config)
    assert len(messages) == 1
    assert "RMT TX memory" in messages[0]


def test_receiver_uses_own_rx_blocks(run_check: CheckRunner) -> None:
    """On S3 the receiver has its own RX blocks and does not take TX memory."""
    config = {**_strips(192), "remote_receiver": [_remote("rx", 192)]}
    assert not run_check(VARIANT_ESP32S3, config)


@pytest.mark.parametrize("with_transmitter", [False, True])
def test_receiver_shares_blocks_on_s2(
    run_check: CheckRunner, with_transmitter: bool
) -> None:
    """On S2 a 64 symbol strip and a 192 symbol receiver fill the four shared blocks."""
    config = {**_strips(64), "remote_receiver": [_remote("rx", 192)]}
    if with_transmitter:
        config["remote_transmitter"] = [_remote("ir", 64)]
    messages = run_check(VARIANT_ESP32S2, config)
    assert len(messages) == with_transmitter
    if with_transmitter:
        assert "RMT TX/RX memory" in messages[0]


def test_dma_channel_uses_one_block(run_check: CheckRunner) -> None:
    """A DMA channel takes one block whatever its rmt_symbols, which size the DMA buffer."""
    lights = [_strip("dma", 1024, **{CONF_USE_DMA: True}), _strip("other", 144)]
    assert not run_check(VARIANT_ESP32S3, {"light": lights})


def test_suggestion_leaves_out_dma_channels(run_check: CheckRunner) -> None:
    """The suggested value splits only the blocks the DMA channel leaves free."""
    lights = [_strip("dma", 192, **{CONF_USE_DMA: True}), _strip("other", 192)]
    messages = run_check(VARIANT_ESP32S3, {"light": lights})
    assert len(messages) == 1
    assert "'dma' (DMA), 'other' (192)" in messages[0]
    assert "Set 'rmt_symbols: 144' on 'other'." in messages[0]


def test_two_dma_channels_in_one_direction_fail(run_check: CheckRunner) -> None:
    """Only the last channel of each direction supports DMA."""
    config = {
        "light": [_strip("dma", 48, **{CONF_USE_DMA: True})],
        "remote_transmitter": [_remote("ir", 48, **{CONF_USE_DMA: True})],
    }
    with pytest.raises(cv.Invalid, match="Only one RMT TX channel on ESP32S3"):
        run_check(VARIANT_ESP32S3, config)


def test_dma_on_tx_and_rx(run_check: CheckRunner) -> None:
    """One DMA channel per direction is fine."""
    config = {
        "light": [_strip("dma", 48, **{CONF_USE_DMA: True})],
        "remote_receiver": [_remote("rx", 48, **{CONF_USE_DMA: True})],
    }
    assert not run_check(VARIANT_ESP32S3, config)


@pytest.mark.parametrize(
    ("variant", "config", "match"),
    [
        (VARIANT_ESP32C3, _strips(48, 48, 48), "ESP32C3 has 2 RMT TX channels"),
        (
            VARIANT_ESP32S2,
            {**_strips(64, 64, 64, 64), "remote_receiver": [_remote("rx", 64)]},
            "ESP32S2 has 4 RMT TX/RX channels, but these 5 components",
        ),
        (
            VARIANT_ESP32S3,
            {"remote_receiver": [_remote(f"rx{i}", 48) for i in range(5)]},
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
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    CORE.data[KEY_ESP32] = {KEY_VARIANT: VARIANT_ESP32S3}
    receiver = _remote("rx", 48)
    transmitters = [_remote(f"tx{i}", 48) for i in range(5)]
    receivers = [
        receiver,
        *(_remote(f"rx{i}", 48, **{CONF_USE_DMA: True}) for i in range(2)),
    ]
    token = fv.full_config.set(
        {"remote_transmitter": transmitters, "remote_receiver": receivers}
    )
    try:
        esp32_rmt.validate_symbol_budget(receiver)
        with pytest.raises(cv.Invalid) as error:
            esp32_rmt.validate_symbol_budget(transmitters[0])
    finally:
        fv.full_config.reset(token)
    # Both errors are reported at once, as final validation stops after the first one
    assert "ESP32S3 has 4 RMT TX channels" in str(error.value)
    assert "Only one RMT RX channel on ESP32S3 supports DMA" in str(error.value)


def test_other_light_platforms_ignored(run_check: CheckRunner) -> None:
    """Lights of other platforms do not count, even with an rmt_symbols key."""
    lights = [_strip("a", 192), {**_strip("b", 192), CONF_PLATFORM: "neopixelbus"}]
    assert not run_check(VARIANT_ESP32S3, {"light": lights})


@pytest.mark.parametrize(
    ("platform", "variant"),
    [
        (PLATFORM_ESP8266, VARIANT_ESP32S3),
        (PLATFORM_ESP32, VARIANT_ESP32C2),
        (PLATFORM_ESP32, "ESP32H4"),
    ],
)
def test_not_checked_without_rmt_caps(
    run_check: CheckRunner, platform: str, variant: str
) -> None:
    """Other platforms, variants without RMT and variants not in the table are skipped."""
    assert not run_check(variant, _strips(192, 192, 192, 192, 192), platform)


def test_not_checked_in_testing_mode(
    run_check: CheckRunner, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Grouped component tests merge many configs, so the check is skipped there."""
    monkeypatch.setattr(CORE, "testing_mode", True)
    assert not run_check(VARIANT_ESP32C3, _strips(96, 96, 96))


def test_check_runs_once(
    run_check: CheckRunner, caplog: pytest.LogCaptureFixture
) -> None:
    """Every RMT component calls the check; it warns once per config."""
    run_check(VARIANT_ESP32S3, _strips(192, 192))
    caplog.clear()
    with caplog.at_level(logging.WARNING):
        esp32_rmt.validate_symbol_budget({})
    assert not caplog.records
