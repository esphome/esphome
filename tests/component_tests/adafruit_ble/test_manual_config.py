"""Public YAML and generated-code contracts for both DFU entry paths."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE


@pytest.mark.parametrize("trigger", [None, True, False])
@pytest.mark.parametrize("server", [False, True])
def test_entry_modes(
    tmp_path: Path,
    component_config_path: Callable[[str], Path],
    generate_main: Callable[[str | Path], str],
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
    trigger: bool | None,
    server: bool,
) -> None:
    text = component_config_path("adafruit_ble_manual.yaml").read_text()
    text = text.replace(
        "    ble_trigger: false\n",
        "" if trigger is None else f"    ble_trigger: {str(trigger).lower()}\n",
    )
    if server:
        text += "\nzephyr_ble_server:\n"
    path = tmp_path / "entry.yaml"
    path.write_text(text)
    CORE.config_path = path
    config = read_config({})
    if trigger is not False and not server:
        assert config is None
        assert "requires component zephyr_ble_server" in capsys.readouterr().out
        return
    assert config is not None
    CORE.reset()
    main = generate_main(path)
    assert "firmware_update->request_dfu_mode();" in main
    assert ("zephyr_ble_server" in CORE.loaded_integrations) is server
    assert ("USE_ADAFRUIT_BLE_TRIGGER" in {d.name for d in CORE.defines}) is (
        trigger is not False
    )
    if trigger is False:
        assert "without pairing" not in caplog.text


@pytest.mark.parametrize(
    "action", ["null", "{}", "firmware_update", "{id: firmware_update}"]
)
def test_action_id_forms(
    tmp_path: Path,
    component_config_path: Callable[[str], Path],
    generate_main: Callable[[str | Path], str],
    action: str,
) -> None:
    text = component_config_path("adafruit_ble_manual.yaml").read_text()
    path = tmp_path / "action.yaml"
    path.write_text(
        text.replace("enter_bootloader: firmware_update", f"enter_bootloader: {action}")
    )
    assert "firmware_update->request_dfu_mode();" in generate_main(path)


@pytest.mark.parametrize(
    ("old", "new", "error"),
    [
        (
            "enter_bootloader: firmware_update",
            "enter_bootloader: missing",
            "Couldn't find ID 'missing'",
        ),
        (
            "enter_bootloader: firmware_update",
            "enter_bootloader: wrong_type",
            "doesn't inherit",
        ),
        (
            "bootloader: adafruit_nrf52_sd140_v6",
            "bootloader: mcuboot",
            "does not support",
        ),
        (
            "board: adafruit_itsybitsy_nrf52840\n  bootloader: adafruit_nrf52_sd140_v6",
            "board: nrf52840dongle\n  bootloader: nrf",
            "requires an Adafruit bootloader",
        ),
        (
            "    ble_trigger: false",
            "    ble_trigger: false\n  - platform: adafruit_ble\n    ble_trigger: true",
            "multiple",
        ),
    ],
)
def test_rejected_config(
    tmp_path: Path,
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
    old: str,
    new: str,
    error: str,
) -> None:
    text = (
        component_config_path("adafruit_ble_manual.yaml")
        .read_text()
        .replace("logger:\n", "logger:\n  id: wrong_type\n")
    )
    path = tmp_path / "invalid.yaml"
    path.write_text(text.replace(old, new) + "\nzephyr_ble_server:\n")
    CORE.config_path = path
    assert read_config({}) is None
    assert error in capsys.readouterr().out


@pytest.mark.parametrize("first", [True, False])
@pytest.mark.parametrize("second", [True, False])
def test_duplicate_instances_rejected(
    tmp_path: Path,
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
    first: bool,
    second: bool,
) -> None:
    text = component_config_path("adafruit_ble_manual.yaml").read_text()
    text = text.replace("ble_trigger: false", f"ble_trigger: {str(first).lower()}")
    text = text.replace(
        "\nbutton:",
        f"\n  - platform: adafruit_ble\n    ble_trigger: {str(second).lower()}\n\nbutton:",
    )
    path = tmp_path / "duplicate.yaml"
    path.write_text(text + "\nzephyr_ble_server:\n")
    CORE.config_path = path
    assert read_config({}) is None
    assert "does not support multiple instances" in capsys.readouterr().out


def test_other_platform_does_not_load_adafruit(
    tmp_path: Path,
    generate_main: Callable[[str | Path], str],
    capsys: pytest.CaptureFixture[str],
) -> None:
    path = tmp_path / "esp32.yaml"
    text = "esphome:\n  name: esp32test\nesp32:\n  board: esp32dev\nlogger:\n"
    path.write_text(text)
    generate_main(path)
    assert "adafruit_ble" not in CORE.loaded_integrations
    assert "USE_ADAFRUIT_BLE_TRIGGER" not in {d.name for d in CORE.defines}
    CORE.reset()
    path.write_text(text + "ota:\n  - platform: adafruit_ble\n    ble_trigger: false\n")
    CORE.config_path = path
    assert read_config({}) is None
    assert "requires component zephyr" in capsys.readouterr().out
