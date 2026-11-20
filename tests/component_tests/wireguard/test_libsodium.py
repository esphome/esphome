"""Tests for where the wireguard library gets libsodium from.

Another component can bring in the registry libsodium without ESPHome knowing
(sendspin-cpp does, through noise-c). The build then fails if the converted
wireguard library also brings its own copy.
"""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.esp32.const import KEY_COMPONENTS, KEY_ESP32, KEY_REF
from esphome.components.noise import LIBSODIUM_VERSION
from esphome.core import CORE

_LIBSODIUM = "esphome/libsodium"


def _libraries() -> set[str]:
    return {library.name for library in CORE.platformio_libraries.values()}


@pytest.mark.parametrize(
    "config",
    [
        "esp32_idf.yaml",
        "esp32_arduino.yaml",
        "esp32_idf_platformio.yaml",
        "esp32_arduino4_platformio.yaml",
    ],
)
def test_declares_the_registry_libsodium(
    config: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path(config))

    components = CORE.data[KEY_ESP32][KEY_COMPONENTS]
    assert components[_LIBSODIUM][KEY_REF] == LIBSODIUM_VERSION
    assert "esphome/wireguard" in _libraries()


def test_arduino_3_on_platformio_keeps_the_library_dependency(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """arduino-esp32's own libsodium cannot be replaced there, so noise does not use
    the registry copy either."""
    generate_main(component_config_path("esp32_arduino_platformio.yaml"))

    assert _LIBSODIUM not in CORE.data[KEY_ESP32][KEY_COMPONENTS]
    assert "esphome/wireguard" in _libraries()


def test_other_platforms_have_no_idf_components(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("esp8266.yaml"))

    assert KEY_ESP32 not in CORE.data
    assert "esphome/wireguard" in _libraries()
