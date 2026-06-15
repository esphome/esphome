"""Tests for the _final_validate buffer size calculation in mipi_spi."""

from __future__ import annotations

from typing import Any

import pytest

from esphome.components.display import CONF_SHOW_TEST_CARD
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.components.mipi_spi.display import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
from esphome.const import CONF_BUFFER_SIZE, PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _validated(config: ConfigType) -> ConfigType:
    """Run the component config schema followed by the final validation."""
    config = CONFIG_SCHEMA(config)
    FINAL_VALIDATE_SCHEMA(config)
    return config


def _custom_config(
    width: int,
    height: int,
    color_depth: str | int | None = None,
    **extra: Any,
) -> ConfigType:
    """Build a minimal valid custom-model config with the given dimensions."""
    config: ConfigType = {
        "model": "custom",
        "dc_pin": 18,
        "dimensions": {"width": width, "height": height},
        "init_sequence": [[0xA0, 0x01]],
    }
    if color_depth is not None:
        config["color_depth"] = color_depth
    config.update(extra)
    return config


# The auto buffer-size selection inside _final_validate targets ~20 kB of
# pixel buffer. For a buffer of ``depth_bytes * width * height``, it picks the
# smallest integer ``x`` in range(2, 8) such that
# ``min(20000, buffer // 4) / buffer >= 1 / x`` (falling back to ``x = 8``).
# The test cases below cover the full range of possible outcomes (1/4 .. 1/8).
@pytest.mark.parametrize(
    ("width", "height", "color_depth", "expected"),
    [
        # 16-bit color depth -- buffer = 2 * width * height
        # 128*160*2 = 40960 B -> fraction = 10240/40960 = 0.25 -> x = 4
        pytest.param(128, 160, "16bit", 1.0 / 4, id="16bit_tiny"),
        # 200*224*2 = 89600 B -> fraction = 20000/89600 ≈ 0.2232 -> x = 5
        pytest.param(200, 224, "16bit", 1.0 / 5, id="16bit_small"),
        # 240*224*2 = 107520 B -> fraction ≈ 0.1860 -> x = 6
        pytest.param(240, 224, "16bit", 1.0 / 6, id="16bit_medium"),
        # 200*320*2 = 128000 B -> fraction = 0.15625 -> x = 7
        pytest.param(200, 320, "16bit", 1.0 / 7, id="16bit_large"),
        # 240*320*2 = 153600 B -> fraction ≈ 0.1302 -> default x = 8
        pytest.param(240, 320, "16bit", 1.0 / 8, id="16bit_xlarge"),
        # 320*480*2 = 307200 B -> fraction ≈ 0.0651 -> default x = 8
        pytest.param(320, 480, "16bit", 1.0 / 8, id="16bit_huge"),
        # 8-bit color depth -- buffer = width * height
        # 320*240 = 76800 B -> fraction = 19200/76800 = 0.25 -> x = 4
        pytest.param(320, 240, "8bit", 1.0 / 4, id="8bit_tiny"),
        # 400*224 = 89600 B -> fraction ≈ 0.2232 -> x = 5
        pytest.param(400, 224, "8bit", 1.0 / 5, id="8bit_small"),
        # 480*224 = 107520 B -> fraction ≈ 0.1860 -> x = 6
        pytest.param(480, 224, "8bit", 1.0 / 6, id="8bit_medium"),
        # 400*320 = 128000 B -> fraction = 0.15625 -> x = 7
        pytest.param(400, 320, "8bit", 1.0 / 7, id="8bit_large"),
        # 480*320 = 153600 B -> fraction ≈ 0.1302 -> default x = 8
        pytest.param(480, 320, "8bit", 1.0 / 8, id="8bit_xlarge"),
    ],
)
def test_buffer_size_auto_selected(
    width: int,
    height: int,
    color_depth: str,
    expected: float,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Without PSRAM or an explicit buffer_size, a fraction is chosen from the display size.

    Without any drawing method and without LVGL, final validation also auto-enables
    ``show_test_card``, which in turn makes the component require a buffer and therefore
    triggers the buffer-size selection path.
    """
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    config = _validated(_custom_config(width, height, color_depth))

    # Sanity check: final validation should have enabled the test card for us,
    # which is what causes the buffer-size calculation to actually run.
    assert config.get(CONF_SHOW_TEST_CARD) is True
    assert config[CONF_BUFFER_SIZE] == pytest.approx(expected)


@pytest.mark.parametrize(
    "buffer_size",
    [0.125, 0.25, 0.5, 1.0],
    ids=["one_eighth", "one_quarter", "half", "full"],
)
def test_explicit_buffer_size_is_preserved(
    buffer_size: float,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """An explicitly configured buffer_size is never overridden by final validation."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    config = _validated(
        _custom_config(240, 320, "16bit", buffer_size=buffer_size),
    )

    assert config[CONF_BUFFER_SIZE] == pytest.approx(buffer_size)


def test_buffer_size_not_set_when_psram_enabled(
    set_core_config: SetCoreConfigCallable,
    set_component_config,
) -> None:
    """When PSRAM is enabled the auto buffer-size selection is skipped."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    # Presence of the psram domain in the full config is what _final_validate checks.
    set_component_config("psram", True)

    config = _validated(_custom_config(240, 320, "16bit"))

    assert CONF_BUFFER_SIZE not in config


def test_buffer_size_not_set_when_buffer_not_required(
    set_core_config: SetCoreConfigCallable,
    set_component_config,
) -> None:
    """With LVGL present and no drawing methods, no buffer fraction is chosen.

    LVGL suppresses the automatic show_test_card injection, which means
    ``requires_buffer`` is False and the early-return branch fires.
    """
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("lvgl", [])

    config = _validated(_custom_config(240, 320, "16bit"))

    assert CONF_BUFFER_SIZE not in config
    # And no test card should have been auto-enabled either.
    assert not config.get(CONF_SHOW_TEST_CARD)


def test_buffer_size_selected_when_lvgl_with_test_card(
    set_core_config: SetCoreConfigCallable,
    set_component_config,
) -> None:
    """LVGL present + an explicit drawing method still triggers buffer sizing.

    When LVGL is enabled, ``show_test_card`` is not injected automatically,
    but users can still request it explicitly -- in that case ``requires_buffer``
    is True and the buffer-size heuristic still runs.
    """
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("lvgl", [])

    # 128x160 @ 16bit -> expected 1/4 (see test_buffer_size_auto_selected).
    config = _validated(
        _custom_config(128, 160, "16bit", show_test_card=True),
    )

    assert config[CONF_BUFFER_SIZE] == pytest.approx(1.0 / 4)
