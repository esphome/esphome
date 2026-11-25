"""Tests for the sensor component."""

from collections.abc import Callable
import re

from esphome.core import CORE
from esphome.cpp_helpers import (
    _CALLBACK_MANAGER_POOL_DOMAIN,
    CALLBACK_MANAGER_POOL_SIZE,
)
from tests.component_tests.helpers import extract_packed_value


def test_sensor_device_class_set(generate_main):
    """
    When the device_class of sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/sensor/test_sensor.yaml")

    # Then: device_class: voltage means packed value must be non-zero
    packed = extract_packed_value(main_cpp, "s_1")
    assert packed != 0


def test_calibration_tables_are_shared_progmem(
    generate_main: Callable[[str], str],
) -> None:
    """Calibration data is a PROGMEM table, shared by filters with identical data."""
    main_cpp = generate_main("tests/component_tests/sensor/test_calibrate.yaml")

    linear = re.findall(
        r"static constexpr std::array<float, 3> (\w+)\[\] PROGMEM", main_cpp
    )
    assert len(linear) == 2  # the two exact filters share one table
    exact, least_squares = linear
    assert main_cpp.count(f"sensor::CalibrateLinearFilter({exact});") == 2
    assert f"sensor::CalibrateLinearFilter({least_squares});" in main_cpp

    poly = re.search(
        r"static constexpr float (\w+)\[\] PROGMEM = \{2\.0f, 1\.0f, 2\.0f\};", main_cpp
    )
    assert poly is not None
    assert f"sensor::CalibratePolynomialFilter({poly.group(1)});" in main_cpp
    # Table names never reuse a user id.
    assert exact != "sensor_calibrate_linear"
    assert "sensor_calibratelinearfilter_id_data" not in (exact, least_squares)


def test_callback_manager_pool_reserves_only_used_managers(
    generate_main: Callable[[str], str],
) -> None:
    """An entity with automations gets static managers, one per add method; one without gets none."""
    generate_main("tests/component_tests/sensor/test_callback_manager_pool.yaml")

    state = CORE.data[_CALLBACK_MANAGER_POOL_DOMAIN]
    assert state.managers == {
        ("with_automation", "add_on_state_callback"),
        ("with_automation", "add_on_raw_state_callback"),
        ("clicked", "add_on_state_callback"),
    }
    pool = [d for d in CORE.defines if d.name == CALLBACK_MANAGER_POOL_SIZE]
    assert [str(d.value) for d in pool] == ["3"]
