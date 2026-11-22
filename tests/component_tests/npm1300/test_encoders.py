"""Tests for the npm1300 register encoders and validators."""

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_ENABLED
from esphome.components.npm1300 import (
    CHARGER_SCHEMA,
    NPM1300_UNSET,
    OUTPUT_VOLTAGE,
    THERMISTOR_NONE,
    VBUS_CURRENT_LIMIT,
    _charger_config,
    _check_thermistor,
    _config,
    _output_config,
    _termination_voltage_code,
    _thermistor_resistance,
    _vbus_current_limit_code,
    thermistor_beta,
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


def test_warm_termination_defaults_to_reset_value() -> None:
    fields = _charger({"charge_current": "100mA", "termination_voltage": "4.1V"})
    assert fields["termination_voltage"] == "6"
    assert fields["warm_termination_voltage"] == "2"


@pytest.mark.parametrize(
    ("config", "warns"),
    [
        ({"termination_voltage": "4.2V"}, True),
        ({"termination_voltage": "3.5V"}, False),
        ({"termination_voltage": "3.6V"}, False),
        ({}, False),
        ({"termination_voltage": "4.2V", "warm_termination_voltage": "4.0V"}, False),
    ],
)
def test_warm_termination_default_warning(
    config: ConfigType, warns: bool, caplog: pytest.LogCaptureFixture
) -> None:
    CHARGER_SCHEMA(config)
    assert ("warm_termination_voltage" in caplog.text) is warns


@pytest.mark.parametrize(("termination", "code"), [("3.5V", "0"), ("3.55V", "1")])
def test_warm_termination_default_not_above_termination(
    termination: str, code: str
) -> None:
    fields = _charger({"termination_voltage": termination})
    assert fields["warm_termination_voltage"] == code


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


def test_charger_needs_thermistor() -> None:
    with pytest.raises(cv.Invalid, match="thermistor_resistance"):
        _check_thermistor({"charger": {}})
    config = {"charger": {}, "thermistor_resistance": THERMISTOR_NONE}
    assert _check_thermistor(config) is config


@pytest.mark.parametrize("resistance", [None, THERMISTOR_NONE])
def test_thermistor_beta_needs_resistance(resistance: str | None) -> None:
    config = {"thermistor_beta": 3380}
    if resistance is not None:
        config["thermistor_resistance"] = resistance
    with pytest.raises(cv.Invalid, match="thermistor_beta"):
        _check_thermistor(config)


@pytest.mark.parametrize(
    ("resistance", "beta"), [(10000.0, 3380), (47000.0, 4050), (100000.0, 4250)]
)
def test_thermistor_beta_default(resistance: float, beta: int) -> None:
    assert thermistor_beta({"thermistor_resistance": resistance}) == beta


def test_thermistor_beta_configured() -> None:
    config = {"thermistor_resistance": 47000.0, "thermistor_beta": 3950}
    assert thermistor_beta(config) == 3950


@pytest.mark.parametrize(
    ("resistance", "code", "beta"),
    [
        (None, str(NPM1300_UNSET), "0"),
        (THERMISTOR_NONE, "0", "0"),
        (100000.0, "3", "4250"),
    ],
)
def test_config_thermistor(
    resistance: str | float | None, code: str, beta: str
) -> None:
    config = {} if resistance is None else {"thermistor_resistance": resistance}
    fields = _fields(_config(config))
    assert fields["thermistor"] == code
    assert fields["thermistor_beta"] == beta


@pytest.mark.parametrize(
    ("value", "result"),
    [
        ("10kOhm", 10000.0),
        ("0.047MOhm", 47000.0),
        ("100000", 100000.0),
        ("None", "none"),
    ],
)
def test_thermistor_resistance(value: str, result: float | str) -> None:
    assert _thermistor_resistance(value) == result


def test_thermistor_resistance_rejected() -> None:
    with pytest.raises(cv.Invalid, match="10kOhm, 47kOhm, 100kOhm or none"):
        _thermistor_resistance("22kOhm")
