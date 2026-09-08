"""Tests for mipi_rgb configuration validation."""

import pytest

from esphome import config_validation as cv

# Importing these registers their pin schemas with pins.PIN_SCHEMA_REGISTRY so that
# models referencing IO-expander-backed pins in their defaults (e.g. the LilyGO
# T-RGB boards via xl9535, SEEED-INDICATOR-D1 via pca9554, or the Waveshare panels
# via ch422g) can be validated by the mipi_rgb CONFIG_SCHEMA in this test.
import esphome.components.ch422g  # noqa: F401
from esphome.components.display import get_display_metadata
from esphome.components.esp32 import (
    KEY_BOARD,
    VARIANT_ESP32C3,
    VARIANT_ESP32P4,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
)
import esphome.components.pca9554  # noqa: F401
import esphome.components.xl9535  # noqa: F401
from esphome.const import (
    CONF_BLUE,
    CONF_DIMENSIONS,
    CONF_GREEN,
    CONF_HEIGHT,
    CONF_INIT_SEQUENCE,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_RED,
    CONF_SWAP_XY,
    CONF_WIDTH,
    KEY_VARIANT,
    PlatformFramework,
)
from tests.component_tests.types import SetCoreConfigCallable

# A generic set of data pins so that models without a default pin assignment
# (e.g. CUSTOM and RPI) still validate.
DATA_PINS = {
    CONF_RED: [1, 2, 3, 4, 5],
    CONF_GREEN: [6, 7, 8, 9, 10, 11],
    CONF_BLUE: [12, 13, 14, 15, 16],
}


def _set_s3(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={
            KEY_BOARD: "esp32-s3-devkitc-1",
            KEY_VARIANT: VARIANT_ESP32S3,
        },
    )


def test_configuration_success(set_core_config: SetCoreConfigCallable) -> None:
    """Every predefined model validates once required defaults are supplied."""
    _set_s3(set_core_config)

    from esphome.components.mipi_rgb.display import CONFIG_SCHEMA, MODELS

    for name, model in MODELS.items():
        config = {"model": name, "data_pins": DATA_PINS, "pclk_pin": 21}
        if model.initsequence is None:
            config[CONF_INIT_SEQUENCE] = [[0xA0, 0x01]]
        if not model.get_default(CONF_WIDTH):
            config[CONF_DIMENSIONS] = {CONF_WIDTH: 480, CONF_HEIGHT: 480}
        CONFIG_SCHEMA(config)


def test_transform_matches_model_support(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The transform schema only accepts the axes a model actually supports."""
    _set_s3(set_core_config)

    from esphome.components.mipi_rgb.display import CONFIG_SCHEMA, MODELS

    # ESP32-8048S070 supports both mirror axes but not swap_xy (RGB displays
    # never support axis swapping).
    model = MODELS["ESP32-8048S070"]
    assert model.transforms == {CONF_MIRROR_X, CONF_MIRROR_Y}

    base = {"model": "ESP32-8048S070", "data_pins": DATA_PINS, "pclk_pin": 21}
    CONFIG_SCHEMA({**base, "transform": {"mirror_x": True, "mirror_y": False}})

    # An unsupported axis may be explicitly disabled (a harmless no-op)...
    CONFIG_SCHEMA(
        {**base, "transform": {"mirror_x": True, "mirror_y": False, "swap_xy": False}}
    )

    # ...but enabling it reports a clear, model-specific error.
    with pytest.raises(cv.Invalid, match="'swap_xy' is not supported by this model"):
        CONFIG_SCHEMA(
            {
                **base,
                "transform": {"mirror_x": True, "mirror_y": False, "swap_xy": True},
            }
        )


def test_st7701s_only_supports_mirror_x(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """ST7701S panels shorter than full height only expose mirror_x.

    mirror_y only works at full height (864px), so the LilyGO 480px panels must
    reject a mirror_y transform.
    """
    _set_s3(set_core_config)

    from esphome.components.mipi_rgb.display import CONFIG_SCHEMA, MODELS

    model = MODELS["T-RGB-2.1"]
    assert model.transforms == {CONF_MIRROR_X}
    assert CONF_SWAP_XY not in model.transforms

    base = {"model": "T-RGB-2.1"}
    CONFIG_SCHEMA({**base, "transform": {"mirror_x": True}})

    with pytest.raises(cv.Invalid, match="'mirror_y' is not supported by this model"):
        CONFIG_SCHEMA({**base, "transform": {"mirror_x": True, "mirror_y": True}})


def test_metadata_records_rotation(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A configured display rotation is recorded in the metadata.

    LVGL relies on this to flag a rotation set in the display config (see the
    mipi_spi tests for the end-to-end LVGL rejection).
    """
    _set_s3(set_core_config)

    from esphome.components.mipi_rgb.display import CONFIG_SCHEMA

    base = {"model": "ESP32-8048S070", "data_pins": DATA_PINS, "pclk_pin": 21}
    config = CONFIG_SCHEMA({**base, "id": "rotated", "rotation": 90})
    assert get_display_metadata(config["id"]).rotation == 90

    config = CONFIG_SCHEMA({**base, "id": "unrotated"})
    assert get_display_metadata(config["id"]).rotation == 0


@pytest.mark.parametrize(
    ("variant", "board", "model"),
    [
        # ESP32-8048S070 is a real Sunton board wired for ESP32-S3 (e.g. its
        # default de_pin is GPIO41, which doesn't exist on S31), so it is
        # only meaningful as a config on that variant.
        (VARIANT_ESP32S3, "esp32-s3-devkitc-1", "ESP32-8048S070"),
        # P4 and S31 use the pin-agnostic CUSTOM model so this only checks
        # that the chip itself is accepted, independent of board wiring.
        (VARIANT_ESP32P4, "esp32-p4-evboard", "CUSTOM"),
        # No dedicated board is registered for ESP32-S31 yet; an unknown board
        # name simply skips per-board pin validation.
        (VARIANT_ESP32S31, "esp32-s31-devkitc", "CUSTOM"),
    ],
)
def test_configuration_succeeds_on_supported_variants(
    variant: str, board: str, model: str, set_core_config: SetCoreConfigCallable
) -> None:
    """mipi_rgb requires a chip with an RGB LCD peripheral: S3, P4 or S31."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: board, KEY_VARIANT: variant},
    )

    from esphome.components.mipi_rgb.display import CONFIG_SCHEMA

    config = {"model": model, "data_pins": DATA_PINS, "pclk_pin": 21}
    if model == "CUSTOM":
        config[CONF_INIT_SEQUENCE] = [[0xA0, 0x01]]
        config[CONF_DIMENSIONS] = {CONF_WIDTH: 480, CONF_HEIGHT: 480}
    CONFIG_SCHEMA(config)


def test_only_on_variant_rejects_unsupported_variant(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A variant without the RGB LCD peripheral (e.g. ESP32-C3) is rejected.

    Exercises the exact ``only_on_variant`` call used by ``mipi_rgb.display``
    directly, since building a full model config with GPIO numbers that are
    also valid on an unsupported variant like ESP32-C3 is unrelated to what
    this checks.
    """
    from esphome.components.esp32 import only_on_variant

    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: VARIANT_ESP32C3},
    )

    validator = only_on_variant(
        supported=[VARIANT_ESP32S3, VARIANT_ESP32P4, VARIANT_ESP32S31]
    )
    with pytest.raises(
        cv.Invalid,
        match=r"This feature is only available on ESP32S3, ESP32P4, ESP32S31",
    ):
        validator({})
