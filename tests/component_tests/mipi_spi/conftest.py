"""Tests for mpip_spi configuration validation."""

from collections.abc import Callable, Generator
from pathlib import Path
from unittest import mock

import pytest

from esphome import config_validation as cv
from esphome.components.display import DisplayMetaData, get_all_display_metadata
from esphome.components.esp32 import KEY_ESP32, KEY_VARIANT, VARIANTS
from esphome.components.esp32.gpio import validate_gpio_pin
from esphome.const import CONF_INPUT, CONF_OUTPUT
from esphome.core import CORE
from esphome.pins import gpio_pin_schema


@pytest.fixture(autouse=True)
def mock_spi_final_validate():
    """Mock spi.final_validate_device_schema since unit tests have no real SPI bus config."""
    with mock.patch(
        "esphome.components.spi.final_validate_device_schema",
        return_value=lambda config: None,
    ):
        yield


@pytest.fixture(scope="session")
def _generation_cache() -> dict[Path, tuple[str, dict[str, DisplayMetaData]]]:
    """Session-wide cache of generation output keyed by fixture yaml path.

    Stores ``(main_cpp_text, display_metadata_snapshot)`` so multiple tests
    that exercise the same fixture only pay for code generation once.
    """
    return {}


@pytest.fixture
def cached_generate_main(
    generate_main: Callable[[str | Path], str],
    _generation_cache: dict[Path, tuple[str, dict[str, DisplayMetaData]]],
) -> Callable[[Path], tuple[str, dict[str, DisplayMetaData]]]:
    """Like ``generate_main`` but caches results across the test session.

    Returns ``(main_cpp_text, display_metadata_snapshot)``; the snapshot is
    captured at generation time so callers see the same data after CORE has
    been reset for the next test.
    """

    def _get(path: Path) -> tuple[str, dict[str, DisplayMetaData]]:
        path = Path(path)
        cached = _generation_cache.get(path)
        if cached is None:
            main_cpp = generate_main(path)
            cached = (main_cpp, dict(get_all_display_metadata()))
            _generation_cache[path] = cached
        return cached

    return _get


@pytest.fixture
def choose_variant_with_pins() -> Generator[Callable[[list], None]]:
    """
    Set the ESP32 variant for the given model based on pins. For ESP32 only since the other platforms
    do not have variants.
    """

    def chooser(pins: list) -> None:
        for variant in VARIANTS:
            try:
                CORE.data[KEY_ESP32][KEY_VARIANT] = variant
                for pin in pins:
                    if pin is not None:
                        pin = gpio_pin_schema(
                            {
                                CONF_INPUT: True,
                                CONF_OUTPUT: True,
                            },
                            internal=True,
                        )(pin)
                        validate_gpio_pin(pin)
                return
            except cv.Invalid:
                continue
        raise cv.Invalid(
            f"No compatible variant found for pins: {', '.join(map(str, pins))}"
        )

    yield chooser
