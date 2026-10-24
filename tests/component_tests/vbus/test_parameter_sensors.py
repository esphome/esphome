"""P2 sensors bind to explicitly polled IDs without changing existing RX models."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.components.vbus import sensor as vbus_sensor
import esphome.config_validation as cv
from esphome.core import CORE


def test_parameter_sensor_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("parameter_sensors.yaml"))
    assert "vbus::VBusP2Sensor(polled_values, 1, true)" in code
    assert "vbus::VBusP2Sensor(polled_values, 2, false)" in code
    assert "vbus::DeltaSolCSPlusSensor()" in code
    assert "MultiplyFilter" in code


@pytest.mark.parametrize("value_id", [-1, 65536])
def test_parameter_id_bounds(value_id: int) -> None:
    with pytest.raises(cv.Invalid):
        vbus_sensor.CONFIG_SCHEMA(
            {
                "model": "p2",
                "polling_id": "poll",
                "value_id": value_id,
                "name": "Parameter",
            }
        )


@pytest.mark.parametrize("value_type", ["int16", "uint16", "float", "int64"])
def test_only_raw_32_bit_value_types_are_supported(value_type: str) -> None:
    with pytest.raises(cv.Invalid):
        vbus_sensor.CONFIG_SCHEMA(
            {
                "model": "p2",
                "polling_id": "poll",
                "value_id": 1,
                "name": "Parameter",
                "value_type": value_type,
            }
        )


@pytest.mark.parametrize(
    "value_id,expected", [(4, "must be registered"), (3, "cannot duplicate")]
)
def test_binding_rejects_unpolled_and_rx_covered_ids(
    value_id: int,
    expected: str,
    tmp_path: Path,
    component_config_path: Callable[[str], Path],
) -> None:
    source = component_config_path("parameter_sensors.yaml").read_text()
    path = tmp_path / "invalid.yaml"
    path.write_text(source.replace("    value_id: 1", f"    value_id: {value_id}", 1))
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    assert any(expected in str(err) for err in result.errors), result.errors


def test_sensor_cannot_bind_to_a_hub_id_instead_of_a_polling_id(
    tmp_path: Path,
    component_config_path: Callable[[str], Path],
) -> None:
    path = tmp_path / "invalid.yaml"
    path.write_text(
        component_config_path("parameter_sensors.yaml")
        .read_text()
        .replace("polling_id: polled_values", "polling_id: bus")
    )
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    assert result.errors
