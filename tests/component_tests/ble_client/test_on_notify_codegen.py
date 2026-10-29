"""Codegen tests for the ble_client sensor and text sensor on_notify wiring."""

from collections.abc import Callable
from pathlib import Path

from tests.component_tests.helpers import get_define_value


def test_on_notify_wiring(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """on_notify turns notifications on and feeds each sensor's own automation (float / std::string)."""
    main_cpp = generate_main(component_config_path("on_notify.yaml"))

    assert "notify_sensor->set_enable_notify(true);" in main_cpp
    assert "notify_text->set_enable_notify(true);" in main_cpp
    assert (
        "notify_sensor->add_on_notify_callback(TriggerForwarder<float>{automation_id});"
        in main_cpp
    )
    assert (
        "notify_text->add_on_notify_callback("
        "TriggerForwarder<std::string>{automation_id_2});" in main_cpp
    )
    # No trigger node is registered on the client for on_notify any more.
    assert main_cpp.count("test_blec->register_ble_node(") == 2
    assert get_define_value("USE_BLE_SENSOR_ON_NOTIFY") is not None
    assert get_define_value("USE_BLE_TEXT_SENSOR_ON_NOTIFY") is not None
