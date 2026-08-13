"""Validation tests for the sendspin image platform.

These cover the rejection branches, which a compile test cannot reach: a
`test*.yaml` can only assert that a configuration is accepted.
"""

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.sendspin import IMAGE_FORMAT_JPEG, MAX_ARTWORK_SLOTS, _get_data
from esphome.components.sendspin.image import CONFIG_SCHEMA, MAX_IMAGE_DIMENSION
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _slot_config(**overrides: Any) -> ConfigType:
    """Build a minimal valid artwork slot config, allowing field overrides."""
    config: ConfigType = {
        "id": "album_slot",
        "format": "JPEG",
        "type": "RGB565",
        "resize": "240x240",
        "current_image": {"id": "album_art"},
    }
    config.update(overrides)
    return config


def test_minimal_config_is_accepted(set_core_config: SetCoreConfigCallable) -> None:
    """The baseline the rejection tests vary is itself valid."""
    set_core_config(PlatformFramework.ESP32_IDF)

    config = CONFIG_SCHEMA(_slot_config())

    assert config["slot"] == 0
    assert config["source"] == "ALBUM"
    assert config["display_offset"].total_milliseconds == 0


@pytest.mark.parametrize("image_format", ["JPEG", "JPG"])
def test_jpeg_alias_maps_to_one_enum(
    set_core_config: SetCoreConfigCallable, image_format: str
) -> None:
    """runtime_image takes JPG as an alias for JPEG, so both spellings must reach the
    library's single JPEG enum."""
    set_core_config(PlatformFramework.ESP32_IDF)

    CONFIG_SCHEMA(_slot_config(format=image_format))

    assert _get_data().artwork_preferences[0]["format"] == IMAGE_FORMAT_JPEG


def test_too_many_slots_rejected(set_core_config: SetCoreConfigCallable) -> None:
    """Slot numbers run out after MAX_ARTWORK_SLOTS entries."""
    set_core_config(PlatformFramework.ESP32_IDF)

    for slot in range(MAX_ARTWORK_SLOTS):
        assert CONFIG_SCHEMA(_slot_config(id=f"slot_{slot}"))["slot"] == slot

    with pytest.raises(cv.Invalid, match="Too many Sendspin image slots"):
        CONFIG_SCHEMA(_slot_config(id="one_too_many"))


@pytest.mark.parametrize(
    "resize",
    [f"{MAX_IMAGE_DIMENSION + 1}x240", f"240x{MAX_IMAGE_DIMENSION + 1}"],
)
def test_oversized_resize_rejected(
    set_core_config: SetCoreConfigCallable, resize: str
) -> None:
    """Either dimension past the decoder's limit is refused."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match=f"must be {MAX_IMAGE_DIMENSION} or less"):
        CONFIG_SCHEMA(_slot_config(resize=resize))


def test_sub_millisecond_display_offset_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The library field is whole milliseconds, so finer values are refused
    rather than silently rounded down to zero."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match="Maximum precision is milliseconds"):
        CONFIG_SCHEMA(_slot_config(display_offset="500us"))


@pytest.mark.parametrize("display_offset", ["61s", "-61s"])
def test_out_of_range_display_offset_rejected(
    set_core_config: SetCoreConfigCallable, display_offset: str
) -> None:
    """Offsets more than a minute either side of the boundary are refused."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match="value must be at (most|least)"):
        CONFIG_SCHEMA(_slot_config(display_offset=display_offset))


@pytest.mark.parametrize(
    ("display_offset", "expected_ms"), [("250ms", 250), ("-2s", -2000)]
)
def test_display_offset_accepted(
    set_core_config: SetCoreConfigCallable, display_offset: str, expected_ms: int
) -> None:
    """Whole-millisecond offsets pass through in both directions."""
    set_core_config(PlatformFramework.ESP32_IDF)

    config = CONFIG_SCHEMA(_slot_config(display_offset=display_offset))

    assert config["display_offset"].total_milliseconds == expected_ms
