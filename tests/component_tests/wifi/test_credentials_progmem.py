"""Tests that wifi codegen keeps the configured credentials in flash on ESP8266."""

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.mark.parametrize("config_file", ["bare.yaml", "bare_ln882x.yaml"])
def test_ssid_and_password_use_flash_literals(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """Codegen emits ESPHOME_F for station and AP credentials; the macro decides placement."""
    main_cpp = generate_main(component_config_path(config_file))

    assert 'set_ssid(ESPHOME_F("test"));' in main_cpp
    assert 'set_password(ESPHOME_F("testtest"));' in main_cpp
    assert 'set_ssid(ESPHOME_F("fallback"));' in main_cpp
    assert 'set_ssid("' not in main_cpp
    assert 'set_password("' not in main_cpp


def test_eap_strings_use_flash_literals(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """EAP identity, username and password are built from flash on ESP8266."""
    main_cpp = generate_main(component_config_path("eap.yaml"))

    for value in ("eap_identity", "eap_username", "eap_password"):
        assert f'progmem_string(ESPHOME_F("{value}"))' in main_cpp


def test_eap_strings_plain_off_esp8266(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Off ESP8266 the EAP strings stay plain literals, with no copy from flash."""
    main_cpp = generate_main(component_config_path("eap_esp32.yaml"))

    assert "progmem_string(" not in main_cpp
    for value in ("eap_identity", "eap_username", "eap_password"):
        assert f'"{value}"' in main_cpp
