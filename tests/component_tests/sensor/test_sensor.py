"""Tests for the sensor component."""

from pathlib import Path
import re

from esphome.config import load_config
from esphome.core import CORE
from esphome.helpers import fnv1_hash_object_id
from tests.component_tests.helpers import extract_packed_value


def test_sensor_device_class_set(generate_main):
    """
    When the device_class of sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/sensor/test_sensor.yaml")

    # Then
    expected_hash = fnv1_hash_object_id("test s1")
    assert f'App.register_sensor(s_1, "test s1", {expected_hash}, ' in main_cpp
    # The `entity_state:` shorthand must carry the YAML source location, like a
    # hand-written lambda would, so a compile error in the generated lambda is reported
    # against the YAML line rather than the generated main.cpp.
    assert re.search(
        r"threshold_id->set_upper_threshold\(\[\]\(\) -> float \{\n"
        r'\s*#line \d+ "tests/component_tests/sensor/test_sensor\.yaml"\n'
        r"\s*return s_1->state;",
        main_cpp,
    )
    # Then: device_class: voltage means packed value must be non-zero
    packed = extract_packed_value(main_cpp, "s_1")
    assert packed != 0


def test_entity_state_shorthand_rejects_id_without_state_member() -> None:
    """`entity_state:` pointing at an id whose type has no `.state` member (a uart bus,
    not any kind of sensor/switch/etc.) must fail config validation, not silently produce
    a lambda that only fails to compile later.
    """
    # Given
    CORE.config_path = Path(
        "tests/component_tests/sensor/test_sensor_entity_state_wrong_type.yaml"
    )

    # When
    result = load_config({})

    # Then
    assert result.errors
    assert any(
        "uart_bus" in str(err) and "is not one of" in str(err) for err in result.errors
    )
