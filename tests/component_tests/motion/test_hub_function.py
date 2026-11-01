"""Tests for the generated motion data publisher."""

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
