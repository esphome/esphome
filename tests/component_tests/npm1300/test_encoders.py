"""Tests for the npm1300 register encoders and validators."""

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_ENABLED
from esphome.components.npm1300 import (
    CHARGER_SCHEMA,
    NPM1300_UNSET,
    OUTPUT_VOLTAGE,
    VBUS_CURRENT_LIMIT,
    _charger_config,
    _output_config,
    _termination_voltage_code,
    _vbus_current_limit_code,
)
from esphome.const import CONF_MODE, CONF_VOLTAGE
from esphome.cpp_generator import StructInitializer
from esphome.types import ConfigType


def _fields(init: StructInitializer) -> dict[str, str]:
    return {key: str(value) for key, value in init.args.items()}


def _charger(config: ConfigType) -> dict[str, str]:
    return _fields(_charger_config(CHARGER_SCHEMA(config)))


@pytest.mark.parametrize(
    ("current", "code"), [("32mA", 16), ("150mA", 75), ("800mA", 400)]
)
def test_charge_current_code(current: str, code: int) -> None:
    assert _charger({"charge_current": current})["current"] == str(code)


@pytest.mark.parametrize("current", ["30mA", "802mA", "151mA"])
def test_charge_current_rejected(current: str) -> None:
    with pytest.raises(cv.Invalid):
        CHARGER_SCHEMA({"charge_current": current})


@pytest.mark.parametrize(
    ("voltage", "code"),
    [(3.5, 0), (3.65, 3), (4.0, 4), (4.2, 8), (4.45, 13)],
)
def test_termination_voltage_code(voltage: float, code: int) -> None:
    assert _termination_voltage_code(voltage) == code


@pytest.mark.parametrize("voltage", ["3.7V", "3.95V", "4.5V"])
def test_termination_voltage_gap_rejected(voltage: str) -> None:
    with pytest.raises(cv.Invalid):
        CHARGER_SCHEMA({"charge_current": "100mA", "termination_voltage": voltage})


def test_warm_termination_defaults_to_termination() -> None:
    fields = _charger({"charge_current": "100mA", "termination_voltage": "4.1V"})
    assert fields["termination_voltage"] == "6"
    assert fields["warm_termination_voltage"] == "6"


def test_warm_termination_code() -> None:
    fields = _charger(
        {
            "charge_current": "100mA",
            "termination_voltage": "4.2V",
            "warm_termination_voltage": "3.6V",
        }
    )
    assert fields["termination_voltage"] == "8"
    assert fields["warm_termination_voltage"] == "2"


@pytest.mark.parametrize(("voltage", "code"), [("1.0V", 0), ("1.8V", 8), ("3.3V", 23)])
def test_output_voltage_code(voltage: str, code: int) -> None:
    config = {CONF_VOLTAGE: OUTPUT_VOLTAGE(voltage)}
    assert _fields(_output_config(config))["voltage"] == str(code)


@pytest.mark.parametrize("voltage", ["0.9V", "3.4V", "1.85V"])
def test_output_voltage_rejected(voltage: str) -> None:
    with pytest.raises(cv.Invalid):
        OUTPUT_VOLTAGE(voltage)


@pytest.mark.parametrize("config", [None, {}])
def test_unset_output_fields(config: ConfigType | None) -> None:
    unset = str(NPM1300_UNSET)
    assert _fields(_output_config(config)) == {
        "voltage": unset,
        "ldo_mode": unset,
        "enabled": unset,
    }


def test_output_mode_and_enabled() -> None:
    fields = _fields(_output_config({CONF_MODE: "load_switch", CONF_ENABLED: True}))
    assert fields["ldo_mode"] == "0"
    assert fields["enabled"] == "1"
    assert fields["voltage"] == str(NPM1300_UNSET)


@pytest.mark.parametrize(
    ("limit", "code"), [("100mA", 1), ("300mA", 3), ("500mA", 5), ("1.5A", 15)]
)
def test_vbus_current_limit_code(limit: str, code: int) -> None:
    assert _vbus_current_limit_code(VBUS_CURRENT_LIMIT(limit)) == code


@pytest.mark.parametrize("limit", ["50mA", "1600mA", "550mA"])
def test_vbus_current_limit_rejected(limit: str) -> None:
    with pytest.raises(cv.Invalid):
        VBUS_CURRENT_LIMIT(limit)
