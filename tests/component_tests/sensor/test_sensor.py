"""Tests for the sensor component."""

from collections.abc import Callable
import re

from esphome.components.sensor import DELTA_VALUE
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


def test_delta_accept_functions_are_shared(
    generate_main: Callable[[str], str],
) -> None:
    """Identical delta configs share one accept function; a baseline lambda never does."""
    main_cpp = generate_main("tests/component_tests/sensor/test_delta.yaml")

    bodies = re.findall(
        rf"static bool (\w+)\(float x, float {DELTA_VALUE}\) \{{\n(.*?)\n\}}",
        main_cpp,
        re.DOTALL,
    )
    calls = sorted(body.strip() for _, body in bodies)
    call = f"return sensor::delta_passes{{}}({DELTA_VALUE}, {{}});"
    # The two identical `delta: 0.1` filters share one function
    assert calls == sorted(
        [
            call.format("", "x, 0.1f"),
            call.format("<false, true>", "x, 1.0f, 0.1f"),
            call.format("<true>", "x, 0.05f"),
            call.format("", "x, 1.0f, 10.0f"),
            call.format("", "0.0f, 0.0f, 10.0f"),
            call.format("", "20.0f, 0.0f, 10.0f"),
            # A baseline lambda is never shared, even when identical
            call.format("", "static_cast<float>(value->state), 0.0f, 10.0f"),
            call.format("", "static_cast<float>(value->state), 0.0f, 10.0f"),
        ]
    )
