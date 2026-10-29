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
        r"static bool (\w+)\(float x, float esphome_delta_value_\) \{\n(.*?)\n\}",
        main_cpp,
        re.DOTALL,
    )
    returns = [body.splitlines()[-1].strip() for _, body in bodies]
    inf = "!sensor::delta_filter_is_inf(esphome_delta_ref_)"
    assert returns == [
        f"return {inf} && esphome_delta_ > 0.1f;",
        (
            f"return {inf} && esphome_delta_ > 1.0f"
            " && esphome_delta_ <= fabsf(esphome_delta_ref_ * 0.1f);"
        ),
        "return esphome_delta_ > fabsf(esphome_delta_ref_ * 0.05f);",
        "return esphome_delta_ > 1.0f && esphome_delta_ <= 10.0f;",
        "return esphome_delta_ > 0.0f && esphome_delta_ <= 10.0f;",
        "return esphome_delta_ > 0.0f && esphome_delta_ <= 10.0f;",
    ]
    refs = [body.splitlines()[0].strip() for _, body in bodies[-2:]]
    assert refs == [
        "const float esphome_delta_ref_ = static_cast<float>(value->state);",
        "const float esphome_delta_ref_ = static_cast<float>(ref->state);",
    ]
    shared = bodies[0][0]
    assert len(re.findall(rf"sensor::DeltaFilter\({shared}\);", main_cpp)) == 2
