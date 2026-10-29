"""Tests for the sensor component."""

from collections.abc import Callable
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


def test_calibration_tables_are_shared_progmem(
    generate_main: Callable[[str], str],
) -> None:
    """Calibration data is a PROGMEM table, shared by filters with identical data."""
    main_cpp = generate_main("tests/component_tests/sensor/test_calibrate.yaml")

    linear = re.findall(
        r"static constexpr std::array<float, 3> (\w+)\[\] PROGMEM", main_cpp
    )
    assert len(linear) == 1  # only the exact filters have a table, and they share it
    (exact,) = linear
    assert main_cpp.count(f"sensor::CalibrateLinearFilter({exact});") == 2
    # A single segment needs no table: its function is baked into an apply filter
    assert "return x * 2.0f + 1.0f;" in main_cpp

    poly = re.search(
        r"static constexpr float (\w+)\[\] PROGMEM = \{2\.0f, 1\.0f, 2\.0f\};", main_cpp
    )
    assert poly is not None
    assert f"sensor::CalibratePolynomialFilter({poly.group(1)});" in main_cpp
    # Table names never reuse a user id.
    assert exact != "sensor_calibrate_linear"
    assert exact != "sensor_calibratelinearfilter_id_data"


def test_clamp_drops_bounds_infinite_as_float(
    generate_main: Callable[[str], str],
) -> None:
    """A bound that overflows a C++ float is no bound, as the filter compares in float."""
    main_cpp = generate_main("tests/component_tests/sensor/test_clamp.yaml")

    assert "if (!(x >= 1.25f))\n    return 1.25f;\n  return x;" in main_cpp
    assert "if (!(x <= 2.75f))\n    return {};\n  return x;" in main_cpp
    assert "3.5e+38f" not in main_cpp
