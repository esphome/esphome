"""Tests for the generated bmi270 magnetometer publisher."""

from collections.abc import Callable
from pathlib import Path


def test_magnetometer_publisher_generated(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Magnetic field sensors share one function; an idle hub gets none."""
    main_cpp = generate_main(component_config_path("hub_function.yaml"))

    assert (
        "static void esphome__imu__magnetometer_fn("
        "const bmi270::bmm150::BMM150Data & esphome__arg_data) {" in main_cpp
    )
    assert "::mag_x->publish_state(esphome__arg_data.x);" in main_cpp
    assert "::mag_z->publish_state(esphome__arg_data.z);" in main_cpp
    assert "imu->set_magnetometer_publisher(esphome__imu__magnetometer_fn);" in main_cpp
    assert "idle_imu->set_magnetometer_publisher(" not in main_cpp
