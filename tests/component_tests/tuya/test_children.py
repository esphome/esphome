"""Tests for the generated registration and dump of the Tuya child entities."""

from collections.abc import Callable
from pathlib import Path


def test_children_register_before_setup_and_dump_from_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Children register in the generated setup code, after their datapoint setters and the hub's automations."""
    main_cpp = generate_main(component_config_path("children.yaml"))

    def at(text: str) -> int:
        assert text in main_cpp, text
        return main_cpp.index(text)

    hub_listener = at("tuya_hub->register_listener(1,")
    for var, setter in (
        ("tuya_sensor", "set_sensor_id(1)"),
        ("tuya_select", "set_select_id(42,"),
    ):
        register = at(f"{var}->register_listeners();")
        assert hub_listener < register
        assert at(f"{var}->{setter}") < register
        assert f"App.register_component_({var}" not in main_cpp
        assert f"::{var}->dump_child_config();" in main_cpp
    assert "tuya_hub->set_dump_publisher(esphome__tuya_hub__dump_fn);" in main_cpp
    # The fan restores its state, so it stays a component
    assert "App.register_component_(tuya_fan" in main_cpp
