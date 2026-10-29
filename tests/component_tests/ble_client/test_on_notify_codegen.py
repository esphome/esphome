"""Codegen tests for the ble_client sensor and text sensor on_notify wiring."""

from collections.abc import Callable
from pathlib import Path
import re


def test_on_notify_wiring(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """on_notify turns notifications on and feeds each sensor's own automation (float / std::string)."""
    main_cpp = generate_main(component_config_path("on_notify.yaml"))

    assert "notify_sensor->set_enable_notify(true);" in main_cpp
    assert "notify_text->set_enable_notify(true);" in main_cpp
    assert "ble_client::BLESensorNotifyTrigger(notify_sensor);" in main_cpp
    assert re.search(
        r"Automation<float>\(ble_client_blesensornotifytrigger_id\);", main_cpp
    )
    assert "ble_client::BLETextSensorNotifyTrigger(notify_text);" in main_cpp
    assert re.search(
        r"Automation<std::string>\(ble_client_bletextsensornotifytrigger_id\);",
        main_cpp,
    )
