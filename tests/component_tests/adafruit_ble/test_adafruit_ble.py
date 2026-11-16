"""Tests for the adafruit_ble OTA platform and its nrf52 integration."""

from collections.abc import Callable, Generator
from copy import deepcopy
import logging
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.adafruit_ble.ota import CONFIG_SCHEMA, _final_validate
from esphome.components.nrf52 import _final_validate as nrf52_final_validate
from esphome.components.nrf52.const import BOOTLOADER_ADAFRUIT_NRF52_SD140_V6
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_ZEPHYR,
)
from esphome.config import Config, read_config
from esphome.const import (
    CONF_ADVANCED,
    CONF_DISABLED,
    CONF_ENABLE_OTA_ROLLBACK,
    CONF_FRAMEWORK,
    CONF_OTA,
    CONF_PLATFORM,
    CONF_SAFE_MODE,
    Toolchain,
)
from esphome.core import CORE
import esphome.final_validate as fv

NRF52_CONFIG = {
    KEY_BOOTLOADER: BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    CONF_FRAMEWORK: {CONF_ADVANCED: {}},
}


@pytest.fixture
def adafruit_ble_ota_full_config() -> Generator[None]:
    """Full config as final validation sees it with adafruit_ble OTA configured."""
    full = Config()
    full[CONF_OTA] = [{CONF_PLATFORM: "adafruit_ble"}]
    full[CONF_SAFE_MODE] = {CONF_DISABLED: False}
    token = fv.full_config.set(full)
    yield
    fv.full_config.reset(token)


@pytest.fixture
def entry_config(
    tmp_path: Path, component_config_path: Callable[[str], Path]
) -> Callable[..., Path]:
    """Adapt the manual fixture without duplicating its configuration."""

    def write(trigger: bool | None = False, server: bool = False) -> Path:
        text = component_config_path("adafruit_ble_manual.yaml").read_text()
        text = text.replace(
            "    ble_trigger: false\n",
            "" if trigger is None else f"    ble_trigger: {str(trigger).lower()}\n",
        )
        if server:
            text += "\nzephyr_ble_server:\n"
        path = tmp_path / "entry.yaml"
        path.write_text(text)
        return path

    return write


@pytest.mark.parametrize(
    ("trigger", "server"), [(None, True), (True, True), (False, False), (False, True)]
)
def test_adafruit_ble_config_builds_ota_component(
    generate_main: Callable[[str | Path], str],
    entry_config: Callable[..., Path],
    trigger: bool | None,
    server: bool,
) -> None:
    main_cpp = generate_main(entry_config(trigger, server))
    assert "adafruit_ble::OTAComponent" in main_cpp
    assert "firmware_update->request_dfu_mode();" in main_cpp
    assert ("zephyr_ble_server" in CORE.loaded_integrations) is server
    assert ("USE_ADAFRUIT_BLE_TRIGGER" in {d.name for d in CORE.defines}) is (
        trigger is not False
    )


@pytest.mark.parametrize("trigger", [None, True])
def test_requires_zephyr_ble_server(
    entry_config: Callable[..., Path],
    capsys: pytest.CaptureFixture[str],
    trigger: bool | None,
) -> None:
    CORE.config_path = entry_config(trigger)
    assert read_config({}) is None
    assert "requires component zephyr_ble_server" in capsys.readouterr().out


@pytest.mark.parametrize("trigger", [True, False])
def test_requires_adafruit_bootloader(trigger: bool) -> None:
    CORE.data[KEY_ZEPHYR] = {KEY_BOOTLOADER: BOOTLOADER_MCUBOOT}
    with pytest.raises(cv.Invalid, match="requires an Adafruit bootloader"):
        _final_validate({"ble_trigger": trigger})


@pytest.mark.parametrize(
    ("server", "trigger", "warned"),
    [
        ({}, None, True),
        ({}, True, True),
        ({"on_numeric_comparison_request": [{"then": []}]}, True, False),
        ({}, False, False),
        (None, False, False),
    ],
    ids=["default_open", "explicit_open", "pairing", "manual_server", "manual"],
)
def test_warns_about_open_trigger_without_pairing(
    adafruit_ble_ota_full_config: None,
    caplog: pytest.LogCaptureFixture,
    server: dict | None,
    trigger: bool | None,
    warned: bool,
) -> None:
    CORE.data[KEY_ZEPHYR] = {KEY_BOOTLOADER: BOOTLOADER_ADAFRUIT_NRF52_SD140_V6}
    if server is not None:
        fv.full_config.get()["zephyr_ble_server"] = server
    with caplog.at_level(logging.WARNING):
        _final_validate({} if trigger is None else {"ble_trigger": trigger})
    assert ("without pairing" in caplog.text) is warned


@pytest.mark.parametrize("key", ["on_progress", "on_end", "on_error", "on_abort"])
def test_rejects_triggers_that_never_fire(key: str) -> None:
    schema = CONFIG_SCHEMA.validators[0]  # Skips the framework check
    with pytest.raises(cv.Invalid, match=f"'{key}' never fires"):
        schema({key: [{"then": []}]})


@pytest.mark.parametrize(
    "drop",
    [None, CONF_SAFE_MODE, CONF_OTA],
    ids=["ota_and_safe_mode", "without_safe_mode", "without_ota"],
)
def test_rollback_ignored_when_enabled_by_user(
    adafruit_ble_ota_full_config: None,
    drop: str | None,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """An explicit request without MCUboot is turned off with a warning, so older configs still validate."""
    if drop is not None:
        del fv.full_config.get()[drop]
    CORE.toolchain = Toolchain.SDK_NRF
    CORE.loaded_integrations = {"adafruit_ble"}
    config = deepcopy(NRF52_CONFIG)
    config[CONF_FRAMEWORK][CONF_ADVANCED][CONF_ENABLE_OTA_ROLLBACK] = True
    nrf52_final_validate(config)
    assert config[CONF_FRAMEWORK][CONF_ADVANCED][CONF_ENABLE_OTA_ROLLBACK] is False
    assert "requires MCUboot" in caplog.text


@pytest.mark.parametrize(
    ("integrations", "expected"),
    [({"adafruit_ble"}, False), ({"zephyr_mcumgr"}, True)],
    ids=["adafruit_ble", "mcumgr_second_stage"],
)
def test_rollback_default_follows_mcuboot(
    adafruit_ble_ota_full_config: None, integrations: set[str], expected: bool
) -> None:
    CORE.toolchain = Toolchain.SDK_NRF
    CORE.loaded_integrations = integrations
    config = deepcopy(NRF52_CONFIG)
    nrf52_final_validate(config)
    assert config[CONF_FRAMEWORK][CONF_ADVANCED][CONF_ENABLE_OTA_ROLLBACK] is expected


def test_rollback_default_off_quietly_without_safe_mode(
    adafruit_ble_ota_full_config: None, caplog: pytest.LogCaptureFixture
) -> None:
    del fv.full_config.get()[CONF_SAFE_MODE]
    CORE.toolchain = Toolchain.SDK_NRF
    CORE.loaded_integrations = {"adafruit_ble"}
    config = deepcopy(NRF52_CONFIG)
    with caplog.at_level(logging.WARNING):
        nrf52_final_validate(config)
    assert config[CONF_FRAMEWORK][CONF_ADVANCED][CONF_ENABLE_OTA_ROLLBACK] is False
    assert "rollback" not in caplog.text


@pytest.mark.parametrize("action", ["firmware_update", "{id: firmware_update}"])
def test_action_id_forms(
    entry_config: Callable[..., Path],
    generate_main: Callable[[str | Path], str],
    action: str,
) -> None:
    path = entry_config()
    path.write_text(
        path.read_text().replace(
            "enter_bootloader: firmware_update", f"enter_bootloader: {action}"
        )
    )
    assert "firmware_update->request_dfu_mode();" in generate_main(path)


@pytest.mark.parametrize(
    ("action", "error"),
    [("missing", "Couldn't find ID 'missing'"), ("wrong_type", "doesn't inherit")],
)
def test_invalid_action_id(
    entry_config: Callable[..., Path],
    capsys: pytest.CaptureFixture[str],
    action: str,
    error: str,
) -> None:
    path = entry_config()
    path.write_text(
        path.read_text()
        .replace("logger:\n", "logger:\n  id: wrong_type\n")
        .replace("enter_bootloader: firmware_update", f"enter_bootloader: {action}")
    )
    CORE.config_path = path
    assert read_config({}) is None
    assert error in capsys.readouterr().out


@pytest.mark.parametrize("first", [True, False])
@pytest.mark.parametrize("second", [True, False])
def test_duplicate_instances_rejected(
    entry_config: Callable[..., Path],
    capsys: pytest.CaptureFixture[str],
    first: bool,
    second: bool,
) -> None:
    path = entry_config(first, server=True)
    path.write_text(
        path.read_text().replace(
            "\nbutton:",
            f"\n  - platform: adafruit_ble\n    ble_trigger: {str(second).lower()}\n\nbutton:",
        )
    )
    CORE.config_path = path
    assert read_config({}) is None
    assert "does not support multiple instances" in capsys.readouterr().out


def test_safe_mode_keeps_the_ble_trigger(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Bluetooth and the trigger come up in safe mode; user automations do not."""
    main_cpp = generate_main(component_config_path("adafruit_ble_safe_mode.yaml"))
    guard = main_cpp.index("->should_enter_safe_mode(")
    for registration in (
        "App.register_component_(zephyr_ble_server_bleserver_id",
        "App.register_component_(adafruit_ble_otacomponent_id",
    ):
        assert main_cpp.index(registration) < guard
    for automation in ("->add_passkey_callback(", "new(ota_otastarttrigger_id)"):
        assert main_cpp.index(automation) > guard
