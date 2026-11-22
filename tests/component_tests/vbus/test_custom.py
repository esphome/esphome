"""Tests for the vbus custom model."""

from collections.abc import Callable
from pathlib import Path


def test_custom_model_without_entity_list(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    """A custom model with no sensors or binary_sensors list still generates."""
    config = tmp_path / "vbus.yaml"
    config.write_text(
        "esphome:\n  name: vbus-custom\n"
        "esp32:\n  board: esp32dev\n  framework:\n    type: esp-idf\n"
        "uart:\n  rx_pin: GPIO16\n  baud_rate: 9600\n"
        "vbus:\n"
        "sensor:\n  - platform: vbus\n    model: custom\n    id: custom_sensors\n"
        "binary_sensor:\n  - platform: vbus\n    model: custom\n    id: custom_binary\n",
        encoding="utf-8",
    )
    main_cpp = generate_main(config)
    assert "custom_sensors->set_sensors({});" in main_cpp
    assert "custom_binary->set_bsensors({});" in main_cpp
