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
    """Station and AP credentials are ESPHOME_F literals; it is a plain literal off ESP8266."""
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
