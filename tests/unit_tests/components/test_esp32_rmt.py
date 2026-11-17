"""Tests for the ESP32 RMT channel and memory budget check.

Channel errors come from final validation; the memory warning comes from code
generation, where the order the components are set up in is known.
"""

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
    CONF_SETUP_PRIORITY,
    CONF_USE_DMA,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE, ID
import esphome.final_validate as fv

# Entries of RMT components, by direction
Config = dict[str, list[dict[str, Any]]]

# Setup priority of the test entries; the same for all, so they are set up in the
# order they are registered
PRIORITY = 600.0

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


def _validate(config: Config) -> None:
    """Validate the entries like ESPHome does: all schemas first, then final validation."""
    for direction, entries in config.items():
        for entry in entries:
            esp32_rmt.request_rmt_channel(direction, PRIORITY)(entry)
    for entries in config.values():
        for entry in entries:
            esp32_rmt.final_validate_rmt_channels(entry)


def _generate(entries: list[dict[str, Any]]) -> None:
    """Register the entries in this order like their to_code does, then run the jobs."""
    for entry in entries:
        esp32_rmt.register_rmt_channel(entry)
    CORE.flush_tasks()


def _rmt_messages(caplog: pytest.LogCaptureFixture) -> list[str]:
    return [r.getMessage() for r in caplog.records if "RMT" in r.getMessage()]


@pytest.fixture
def run_check(caplog: pytest.LogCaptureFixture) -> CheckRunner:
    """Validate the entries, then generate code with them registered in config order."""

    def run(variant: str, config: Config, platform: str = PLATFORM_ESP32) -> list[str]:
        _set_variant(variant, platform)
        caplog.clear()
        with caplog.at_level(logging.WARNING):
            _validate(config)
            _generate([entry for entries in config.values() for entry in entries])
        return _rmt_messages(caplog)

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
    assert (
        "'NeoPixel Port' will not fit: 'Status LEDs' (192), 'NeoPixel Port' (192)"
        in (messages[0])
    )
    assert "Set 'rmt_symbols: 128' on 'Status LEDs', 'NeoPixel Port'." in messages[0]
    assert "ESP-IDF logs 'no free tx channels' at boot" in messages[0]


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
    ir = _entry("ir", 48, **{CONF_SETUP_PRIORITY: PRIORITY - 1})
    messages = run_check(VARIANT_ESP32S3, {"TX": [*_strips(192)["TX"], ir]})
    assert len(messages) == 1
    assert (
        "in the order ESPHome sets up these components, 'ir' will not fit"
        in (messages[0])
    )
    assert "'no free tx channels'" in messages[0]
    assert "no free rx channels" not in messages[0]


@pytest.mark.parametrize(
    ("variant", "symbols"), [(VARIANT_ESP32S3, 384), (VARIANT_ESP32C3, 144)]
)
def test_single_transmitter_may_use_rx_blocks(
    run_check: CheckRunner, variant: str, symbols: int
) -> None:
    """Without receivers, one TX channel can extend into the RX blocks and always fits."""
    assert not run_check(variant, _strips(symbols))


def test_suggestion_when_blocks_do_not_divide_evenly(run_check: CheckRunner) -> None:
    """Three strips share the four S2 blocks: one block each, a whole and even value."""
    messages = run_check(VARIANT_ESP32S2, _strips(192, 192, 192))
    assert "Set 'rmt_symbols: 64' on 'strip0', 'strip1', 'strip2'." in messages[0]


def test_suggestion_skips_component_at_suggested_value(run_check: CheckRunner) -> None:
    """A component that already uses the suggested value is not listed."""
    messages = run_check(VARIANT_ESP32, _strips(192, 192, 192, 128))
    assert "Set 'rmt_symbols: 128' on 'strip0', 'strip1', 'strip2'." in messages[0]


def test_receiver_fails_when_transmitter_takes_rx_blocks(
    run_check: CheckRunner,
) -> None:
    """A TX channel set up first can take the RX blocks, then the receiver fails."""
    config = {**_strips(384), "RX": [_entry("rx", 48)]}
    messages = run_check(VARIANT_ESP32S3, config)
    assert len(messages) == 1
    assert "'rx' will not fit" in messages[0]
    assert "ESP-IDF logs 'no free rx channels' at boot" in messages[0]
    assert "no free tx channels" not in messages[0]


@pytest.mark.parametrize(
    ("symbols", "fails"),
    [
        # The 48 symbol strip is set up first and takes block 0, the other blocks 1 to 6
        ((48, 288), None),
        # The other way round, the 288 symbol strip takes blocks 0 to 5 and with them
        # every TX channel the 48 symbol strip could start at
        ((288, 48), "strip1"),
    ],
)
def test_order_of_transmitters_decides(
    run_check: CheckRunner, symbols: tuple[int, ...], fails: str | None
) -> None:
    """Without receivers TX channels can use the RX blocks; whether they fit depends on order."""
    first, second = _strips(*symbols)["TX"]
    first[CONF_SETUP_PRIORITY] = PRIORITY + 1
    messages = run_check(VARIANT_ESP32S3, {"TX": [first, second]})
    if fails is None:
        assert not messages
    else:
        assert f"'{fails}' will not fit" in messages[0]


def test_registration_order_decides(caplog: pytest.LogCaptureFixture) -> None:
    """Components of the same priority are set up in the order to_code registers them."""
    _set_variant(VARIANT_ESP32S3)
    small, large = _strips(48, 288)["TX"]
    with caplog.at_level(logging.WARNING):
        _validate({"TX": [small, large]})
        # Registered the other way round than in the config
        _generate([large, small])
    messages = _rmt_messages(caplog)
    assert len(messages) == 1
    assert (
        "in the order ESPHome sets up these components, 'strip0' will not fit: "
        "'strip1' (288), 'strip0' (48)" in messages[0]
    )
    assert (
        "Set 'rmt_symbols: 96' on 'strip1', or set 'setup_priority' so the smaller "
        "ones are set up first." in messages[0]
    )


def test_large_strip_first_on_c6_warns(run_check: CheckRunner) -> None:
    """96 then 48 symbols on C6: the first strip takes both TX channels (hardware tested)."""
    messages = run_check(VARIANT_ESP32C6, _strips(96, 48))
    assert len(messages) == 1
    assert (
        "in the order ESPHome sets up these components, 'strip1' will not fit"
        in messages[0]
    )
    assert (
        "or set 'setup_priority' so the smaller ones are set up first" in (messages[0])
    )


def test_small_strip_first_on_c6_fits(run_check: CheckRunner) -> None:
    """48 then 96 symbols on C6: the second strip extends into the RX blocks."""
    assert not run_check(VARIANT_ESP32C6, _strips(48, 96))


def test_no_order_fits(run_check: CheckRunner) -> None:
    """When no order fits, setup_priority is not suggested."""
    messages = run_check(VARIANT_ESP32S2, _strips(192, 192))
    assert (
        "in the order ESPHome sets up these components, 'strip1' will not"
        in (messages[0])
    )
    assert "setup_priority" not in messages[0]


def test_setup_priority_changes_the_order(run_check: CheckRunner) -> None:
    """A component's setup_priority moves it ahead of the others."""
    small = {**_entry("small", 48), CONF_SETUP_PRIORITY: 700.0}
    # Without the override, the large strip would come first and the small one fail
    assert not run_check(VARIANT_ESP32S3, {"TX": [_entry("large", 288), small]})


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
    assert len(messages) == (1 if with_transmitter else 0)
    if with_transmitter:
        assert "'rx' will not fit" in messages[0]


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
        (VARIANT_ESP32, "TX", 48, {}, "even number of at least 64 on ESP32,"),
        (VARIANT_ESP32S3, "RX", 97, {}, "even number of at least 48 on ESP32S3,"),
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
        esp32_rmt.request_rmt_channel(direction, PRIORITY)(
            _entry("x", symbols, **extra)
        )
    assert error.value.path == [CONF_RMT_SYMBOLS]


@pytest.mark.parametrize("variant", sorted(esp32_rmt.RMT_MEMORY))
def test_symbol_limits_of_every_variant(variant: str) -> None:
    """One block is the minimum; TX can reach the RX blocks, RX only its own blocks."""
    memory = esp32_rmt.RMT_MEMORY[variant]
    size = memory.symbols_per_block
    rx_blocks = memory.rx_blocks or 0
    limits = {
        "TX": (memory.tx_blocks + rx_blocks) * size,
        "RX": (rx_blocks or memory.tx_blocks) * size,
    }
    _set_variant(variant)
    for direction, limit in limits.items():
        check = esp32_rmt.request_rmt_channel(direction, PRIORITY)
        for symbols in (size, limit):
            check(_entry("ok", symbols))
        for symbols in (size - 2, limit + 2):
            with pytest.raises(cv.Invalid):
                check(_entry("too_small_or_large", symbols))


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
    assert esp32_rmt.request_rmt_channel(direction, PRIORITY)(entry) is entry


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


def test_combined_errors_are_not_shown_on_use_dma(run_check: CheckRunner) -> None:
    """With errors of other components joined in, the error is on the component itself."""
    config = {
        "TX": [_entry(f"dma{i}", 48, **{CONF_USE_DMA: True}) for i in range(2)],
        "RX": [_entry(f"rx{i}", 48) for i in range(5)],
    }
    with pytest.raises(cv.Invalid, match="supports DMA") as error:
        run_check(VARIANT_ESP32S3, config)
    assert error.value.path == []


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
        (VARIANT_ESP32C3, _strips(48, 48, 48), "ESP32C3 has 2 RMT channels for TX"),
        (
            VARIANT_ESP32S2,
            {**_strips(64, 64, 64, 64), "RX": [_entry("rx", 64)]},
            "ESP32S2 has 4 RMT channels shared by TX and RX, but these 5 components",
        ),
        (
            VARIANT_ESP32S3,
            {"RX": [_entry(f"rx{i}", 48) for i in range(5)]},
            "ESP32S3 has 4 RMT channels for RX",
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
        esp32_rmt.request_rmt_channel("TX", PRIORITY)(entry)
    for entry in receivers:
        esp32_rmt.request_rmt_channel("RX", PRIORITY)(entry)
    esp32_rmt.final_validate_rmt_channels(receiver)
    with pytest.raises(cv.Invalid) as error:
        esp32_rmt.final_validate_rmt_channels(transmitters[0])
    # Both errors are reported at once, as final validation stops after the first one
    assert "ESP32S3 has 4 RMT channels for TX" in str(error.value)
    assert "Only one RMT RX channel on ESP32S3 supports DMA" in str(error.value)


def test_error_raised_when_ids_are_copies() -> None:
    """IDs are compared by value, so a copied ID still finds its error."""
    _set_variant(VARIANT_ESP32C3)
    transmitters = [_entry(f"tx{i}", 48) for i in range(3)]
    for entry in transmitters:
        esp32_rmt.request_rmt_channel("TX", PRIORITY)(entry)
    with pytest.raises(cv.Invalid, match="ESP32C3 has 2 RMT channels for TX"):
        esp32_rmt.final_validate_rmt_channels(
            {**transmitters[0], CONF_ID: transmitters[0][CONF_ID].copy()}
        )


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


def test_warning_comes_from_code_generation(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Validation alone does not warn; code generation warns once."""
    _set_variant(VARIANT_ESP32S3)
    strips = _strips(192, 192)["TX"]
    with caplog.at_level(logging.WARNING):
        _validate({"TX": strips})
        assert not _rmt_messages(caplog)
        _generate(strips)
    assert len(_rmt_messages(caplog)) == 1


@pytest.mark.parametrize(
    ("path", "label"),
    [
        (["light", 1, CONF_ID], "light[1]"),
        # A component that is not a list, such as an external one without MULTI_CONF
        (["my_rmt", CONF_ID], "generated"),
    ],
)
def test_generated_ids_are_named_by_position(
    run_check: CheckRunner, path: list[str | int], label: str
) -> None:
    """A component without a name or manual id is named by where it is in the config."""

    class FullConfig:
        def get_path_for_id(self, _: ID) -> list[str | int]:
            return path

    token = fv.full_config.set(FullConfig())
    try:
        entry = {**_entry("x", 192), CONF_ID: ID("generated", is_manual=False)}
        messages = run_check(VARIANT_ESP32S3, {"TX": [entry, _entry("other", 192)]})
    finally:
        fv.full_config.reset(token)
    assert f"'{label}' (192), 'other' (192)" in messages[0]
