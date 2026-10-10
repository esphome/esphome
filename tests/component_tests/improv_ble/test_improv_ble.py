"""improv_ble is platform neutral; only its BLE server backends are not.

Covers the platform gate (BLE_SERVER_BACKENDS) and the esp32_improv alias that
keeps pre-rename configurations working.
"""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE


def test_esp32_generates_component(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("esp32.yaml"))
    assert "improv_ble::ImprovBLEComponent" in main_cpp


def test_legacy_key_routes_to_improv_ble(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    main_cpp = generate_main(component_config_path("legacy_key.yaml"))
    assert "improv_ble::ImprovBLEComponent" in main_cpp
    assert "'esp32_improv:' top-level key is deprecated" in caplog.text


def test_platform_without_ble_server_rejected(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    # AUTO_LOAD finds no backend for esp8266 and pulls in improv_base only, so
    # the platform gate in CONFIG_SCHEMA is what has to reject the config.
    CORE.config_path = component_config_path("esp8266.yaml")
    assert read_config({}) is None
    assert "only available on" in capsys.readouterr().out


def test_automations_register_state_callbacks(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("automations.yaml"))
    assert "Trigger" not in main_cpp
    for state in (
        "improv::STATE_PROVISIONED",
        "improv::STATE_PROVISIONING",
        "improv::STATE_AUTHORIZED",
        "improv::STATE_STOPPED",
    ):
        assert f"state == {state}" in main_cpp
    assert main_cpp.count("add_on_state_callback(") == 5
    assert "->is_failed()" in main_cpp
    assert "set_authorizer" in main_cpp
    assert "set_status_indicator" in main_cpp
