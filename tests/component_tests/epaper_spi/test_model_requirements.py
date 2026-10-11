"""Tests for the SSD1677 border_waveform option and EpaperModel.check_requirements()."""

from collections.abc import Callable, Generator
from pathlib import Path
import re
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.epaper_spi.display import CONFIG_SCHEMA, MODELS
from esphome.components.epaper_spi.models import EpaperModel
from esphome.components.epaper_spi.models.ssd1677 import CONF_BORDER_WAVEFORM
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)
from esphome.const import PlatformFramework
from esphome.core import CORE
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _ssd1677_config(**overrides: Any) -> ConfigType:
    config: ConfigType = {
        "id": "test_display",
        "model": "ssd1677",
        "dc_pin": 21,
        "busy_pin": 22,
        "reset_pin": 23,
        "cs_pin": 5,
        "dimensions": {"width": 200, "height": 200},
    }
    config.update(overrides)
    return config


def _setup_esp32(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
    variant: str = VARIANT_ESP32,
    board: str = "esp32dev",
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: board, KEY_VARIANT: variant},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})


@pytest.fixture
def temp_model() -> Generator[Callable[..., EpaperModel]]:
    """Register a throwaway EpaperModel for a test and remove it from the shared registry after."""
    created: list[EpaperModel] = []

    def _make(name: str, **defaults: Any) -> EpaperModel:
        model = EpaperModel(name, class_name="EPaperMono", **defaults)
        created.append(model)
        return model

    yield _make
    for model in created:
        MODELS.pop(model.name, None)


# --- border_waveform ---------------------------------------------------------


def test_border_waveform_default_mono(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """ssd1677 defaults border_waveform to 0x01."""
    _setup_esp32(set_core_config, set_component_config)

    result = CONFIG_SCHEMA(_ssd1677_config())

    assert result[CONF_BORDER_WAVEFORM] == 0x01


def test_border_waveform_default_gray4(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The 4-level grayscale variant defaults border_waveform to 0x00, independently of mono."""
    _setup_esp32(
        set_core_config, set_component_config, VARIANT_ESP32S3, "esp32-s3-devkitc-1"
    )
    CORE.raw_config = {"psram": {}}

    result = CONFIG_SCHEMA(
        {"id": "test_display", "model": "seeed-reterminal-sticky-gray4"}
    )

    assert result[CONF_BORDER_WAVEFORM] == 0x00


def test_border_waveform_override(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """An explicit border_waveform overrides the model default."""
    _setup_esp32(set_core_config, set_component_config)

    result = CONFIG_SCHEMA(_ssd1677_config(border_waveform=0x1A))

    assert result[CONF_BORDER_WAVEFORM] == 0x1A


def test_border_waveform_accepts_hex_string(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """border_waveform accepts a hex string like the YAML author would write."""
    _setup_esp32(set_core_config, set_component_config)

    result = CONFIG_SCHEMA(_ssd1677_config(border_waveform="0x1A"))

    assert result[CONF_BORDER_WAVEFORM] == 0x1A


def test_border_waveform_out_of_range(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """border_waveform rejects values that don't fit in a byte."""
    _setup_esp32(set_core_config, set_component_config)

    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_ssd1677_config(border_waveform=0x100))


def test_border_waveform_in_generated_init_sequence(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The configured border_waveform byte reaches the generated init sequence.

    Command 0x3C (60) is followed by a length of 1 and the waveform byte.
    """
    main_cpp = generate_main(component_config_path("ssd1677_border_waveform_test.yaml"))

    assert re.search(r"60,\s*1,\s*0x1A,", main_cpp)


# --- full_update_every / supports_partial_update ------------------------------


def test_full_update_every_rejected_for_gray4(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The gray4 driver's partial updates are black and white and flatten the whole
    panel, so full_update_every > 1 is refused unless that is explicitly accepted."""
    _setup_esp32(
        set_core_config, set_component_config, VARIANT_ESP32S3, "esp32-s3-devkitc-1"
    )
    CORE.raw_config = {"psram": {}}

    with pytest.raises(cv.Invalid, match="monochrome_partial_updates: true"):
        CONFIG_SCHEMA(
            {
                "id": "test_display",
                "model": "seeed-reterminal-sticky-gray4",
                "full_update_every": 5,
            }
        )


def test_full_update_every_accepted_for_gray4_with_monochrome_partials(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """With the trade-off accepted, the gray4 driver takes full_update_every > 1."""
    _setup_esp32(
        set_core_config, set_component_config, VARIANT_ESP32S3, "esp32-s3-devkitc-1"
    )
    CORE.raw_config = {"psram": {}}

    result = CONFIG_SCHEMA(
        {
            "id": "test_display",
            "model": "seeed-reterminal-sticky-gray4",
            "full_update_every": 5,
            "monochrome_partial_updates": True,
        }
    )

    assert result["full_update_every"] == 5


def test_monochrome_partial_updates_not_offered_for_mono_sticky(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The option only exists where partial updates lose something."""
    _setup_esp32(
        set_core_config, set_component_config, VARIANT_ESP32S3, "esp32-s3-devkitc-1"
    )
    CORE.raw_config = {"psram": {}}

    with pytest.raises(cv.Invalid, match="monochrome_partial_updates"):
        CONFIG_SCHEMA(
            {
                "id": "test_display",
                "model": "seeed-reterminal-sticky",
                "monochrome_partial_updates": True,
            }
        )


def test_full_update_every_default_accepted_for_gray4(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Leaving full_update_every at its default of 1 is fine for the gray4 driver."""
    _setup_esp32(
        set_core_config, set_component_config, VARIANT_ESP32S3, "esp32-s3-devkitc-1"
    )
    CORE.raw_config = {"psram": {}}

    result = CONFIG_SCHEMA(
        {"id": "test_display", "model": "seeed-reterminal-sticky-gray4"}
    )

    assert result["full_update_every"] == 1


def test_full_update_every_accepted_for_mono_sticky(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The mono sibling model supports partial update, unaffected by the gray4 restriction."""
    _setup_esp32(
        set_core_config, set_component_config, VARIANT_ESP32S3, "esp32-s3-devkitc-1"
    )
    CORE.raw_config = {"psram": {}}

    result = CONFIG_SCHEMA(
        {
            "id": "test_display",
            "model": "seeed-reterminal-sticky",
            "full_update_every": 5,
        }
    )

    assert result["full_update_every"] == 5


# --- check_requirements -------------------------------------------------------


def test_requirement_missing_raises(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """seeed-reterminal-sticky requires psram; without it, config validation fails."""
    _setup_esp32(
        set_core_config, set_component_config, VARIANT_ESP32S3, "esp32-s3-devkitc-1"
    )
    CORE.raw_config = {}

    with pytest.raises(cv.Invalid, match="requires component 'psram'"):
        CONFIG_SCHEMA({"id": "test_display", "model": "seeed-reterminal-sticky"})


def test_requirement_satisfied_does_not_raise(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """With psram present at the top level, seeed-reterminal-sticky validates."""
    _setup_esp32(
        set_core_config, set_component_config, VARIANT_ESP32S3, "esp32-s3-devkitc-1"
    )
    CORE.raw_config = {"psram": {}}

    result = CONFIG_SCHEMA({"id": "test_display", "model": "seeed-reterminal-sticky"})

    assert result["model"] == "SEEED-RETERMINAL-STICKY"


def test_requirement_check_skipped_without_raw_config(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """With no raw_config (e.g. a schema invoked directly, as in these tests), the
    requirement check is a no-op rather than a false failure."""
    _setup_esp32(
        set_core_config, set_component_config, VARIANT_ESP32S3, "esp32-s3-devkitc-1"
    )
    assert CORE.raw_config is None

    # Should not raise even though "psram" is required and nothing was configured.
    CONFIG_SCHEMA({"id": "test_display", "model": "seeed-reterminal-sticky"})


def test_requirement_missing_multiple_pluralised(
    temp_model: Callable[..., EpaperModel],
) -> None:
    """The error message pluralises "component(s)" and lists every missing one."""
    model = temp_model("test-multi-requirement", requires={"aaa", "bbb"})
    CORE.raw_config = {}

    with pytest.raises(cv.Invalid, match="requires components 'aaa', 'bbb'"):
        model.check_requirements()


# --- width_multiple -----------------------------------------------------------


@pytest.mark.parametrize("width", [804, 801])
def test_gray4_width_not_multiple_of_8_rejected(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
    width: int,
) -> None:
    """The gray4 plane split reads two whole buffer bytes per plane byte, so width must be a multiple of 8."""
    _setup_esp32(
        set_core_config, set_component_config, VARIANT_ESP32S3, "esp32-s3-devkitc-1"
    )

    with pytest.raises(cv.Invalid, match="multiple of 8"):
        CONFIG_SCHEMA(
            {
                "id": "test_display",
                "model": "seeed-reterminal-sticky-gray4",
                "dimensions": {"width": width, "height": 480},
            }
        )


def test_gray4_width_multiple_of_8_accepted(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    _setup_esp32(
        set_core_config, set_component_config, VARIANT_ESP32S3, "esp32-s3-devkitc-1"
    )

    result = CONFIG_SCHEMA(
        {
            "id": "test_display",
            "model": "seeed-reterminal-sticky-gray4",
            "dimensions": {"width": 808, "height": 480},
        }
    )

    assert result["dimensions"]["width"] == 808


def test_mono_ssd1677_accepts_any_width(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The width restriction applies only to the gray4 model."""
    _setup_esp32(set_core_config, set_component_config)

    result = CONFIG_SCHEMA(_ssd1677_config(dimensions={"width": 204, "height": 200}))

    assert result["dimensions"]["width"] == 204


# --- extend(class_name=...) --------------------------------------------------


def test_gray4_code_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """seeed-reterminal-sticky-gray4 generates the EPaperSSD1677Gray4 driver, not EPaperMono."""
    main_cpp = generate_main(component_config_path("ssd1677_gray4_test.yaml"))

    assert "epaper_spi::EPaperSSD1677Gray4" in main_cpp
    assert "epaper_spi::EPaperMono" not in main_cpp
