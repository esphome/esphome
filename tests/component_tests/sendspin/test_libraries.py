"""Tests for the registry components a sendspin build declares."""

from collections.abc import Callable
from pathlib import Path

from esphome.components.esp32.const import KEY_COMPONENTS, KEY_ESP32, KEY_REF
from esphome.components.noise import LIBSODIUM_VERSION, NOISE_C_VERSION
from esphome.core import CORE


def test_noise_c_and_libsodium_are_pinned(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The hub loads the noise component, which pins both. sendspin-cpp accepts any
    noise-c from 0.1.13 up, so a build without API encryption would otherwise take
    whatever version is newest."""
    generate_main(component_config_path("device_info_default.yaml"))

    components = CORE.data[KEY_ESP32][KEY_COMPONENTS]
    assert components["esphome/noise-c"][KEY_REF] == NOISE_C_VERSION
    assert components["esphome/libsodium"][KEY_REF] == LIBSODIUM_VERSION
