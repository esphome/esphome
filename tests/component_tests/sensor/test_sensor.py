"""Tests for the sensor component."""

import re

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


def test_calibration_tables_are_shared_progmem(generate_main) -> None:
    """Calibration data is a PROGMEM table, shared by filters with identical data."""
    main_cpp = generate_main("tests/component_tests/sensor/test_calibrate.yaml")

    linear = re.findall(
        r"static constexpr std::array<float, 3> (\w+)\[\] PROGMEM", main_cpp
    )
    assert len(linear) == 2  # the two exact filters share one table
    exact, least_squares = linear
    assert main_cpp.count(f"sensor::CalibrateLinearFilter({exact}, 2);") == 2
    assert f"sensor::CalibrateLinearFilter({least_squares}, 1);" in main_cpp

    poly = re.search(
        r"static constexpr float (\w+)\[\] PROGMEM = \{1\.0f, 2\.0f\};", main_cpp
    )
    assert poly is not None
    assert f"sensor::CalibratePolynomialFilter({poly.group(1)}, 2);" in main_cpp
    assert "CalibrateLinearFilter<" not in main_cpp
    assert "CalibratePolynomialFilter<" not in main_cpp
