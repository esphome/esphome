"""Tests for the generated motion data and temperature publishers."""

from collections.abc import Callable
from pathlib import Path


def test_data_publisher_generated(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Sensors, binary sensors and events share one function; an idle hub gets none."""
    main_cpp = generate_main(component_config_path("hub_function.yaml"))

    assert (
        "static void esphome__imu__data_fn("
        "const motion::MotionData & esphome__arg_data) {" in main_cpp
    )
    assert "::accel_x->publish_state(esphome__arg_data.acceleration[0]);" in main_cpp
    assert "::imu_roll->publish_state((std::atan2(" in main_cpp
    assert "::face_up->process_motion_data(esphome__arg_data);" in main_cpp
    assert "::shake->process_motion_data(esphome__arg_data);" in main_cpp
    assert "imu->set_data_publisher(esphome__imu__data_fn);" in main_cpp
    assert "idle_imu->set_data_publisher(" not in main_cpp


def test_temperature_publisher_generated(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Each IMU with a temperature sensor gets a function; an idle hub gets none."""
    main_cpp = generate_main(component_config_path("temperature.yaml"))
    for platform, accuracy_decimals in (
        ("lis3dh", 1),
        ("bmi270", 2),
        ("icm20689", 2),
        ("qmi8658", 2),
        ("lsm6ds", 2),
    ):
        fn = f"esphome__{platform}_imu__temperature_fn"
        assert f"static void {fn}(float esphome__arg_temperature) {{" in main_cpp
        assert (
            f"::{platform}_temperature->publish_state(esphome__arg_temperature);"
            in main_cpp
        )
        assert f"{platform}_imu->set_temperature_publisher({fn});" in main_cpp
        assert f"{platform}_idle->set_temperature_publisher(" not in main_cpp
        assert (
            f"{platform}_temperature->set_accuracy_decimals({accuracy_decimals});"
            in main_cpp
        )
