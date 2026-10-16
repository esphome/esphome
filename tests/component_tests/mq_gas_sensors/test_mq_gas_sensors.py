"""Tests for the MQ gas sensors code generation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.mq_gas_sensors.coefficients import SENSOR_TYPES, resolve_sensor
from esphome.components.mq_gas_sensors.sensor import CONFIG_SCHEMA


def test_voltage_source_is_a_constructor_argument(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Every reading needs the sampler, so it is passed at construction."""
    main_cpp = generate_main(component_config_path("mq_gas_sensors.yaml"))

    assert "new(mq8_boot) mq_gas_sensors::MQGasSensor(mq8_adc);" in main_cpp
    assert "new(mq4_fixed) mq_gas_sensors::MQGasSensor(mq8_adc);" in main_cpp
    assert "set_source(" not in main_cpp


def test_built_in_curve_reaches_the_sensor(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The coefficients of the configured type/gas are compiled in."""
    main_cpp = generate_main(component_config_path("mq_gas_sensors.yaml"))

    assert 'mq8_boot->set_type("MQ-8");' in main_cpp
    assert 'mq8_boot->set_gas("H2");' in main_cpp
    assert "mq8_boot->set_a(976.97f);" in main_cpp
    assert "mq8_boot->set_b(-0.688f);" in main_cpp
    assert "mq8_boot->set_regression_method(1);" in main_cpp
    assert "mq8_boot->set_ratio_in_clean_air(70.0f);" in main_cpp
    assert "mq8_boot->set_voltage_multiplier(1.5f);" in main_cpp
    assert "mq8_boot->set_samples(4);" in main_cpp
    assert "mq8_boot->set_calibration(5000, 5, 20);" in main_cpp
    # A fixed R0 skips the boot calibration.
    assert "mq4_fixed->set_r0(10.0f);" in main_cpp
    assert "mq4_fixed->set_calibration(" not in main_cpp
    # The primary gas is assumed when `gas:` is not written.
    assert 'mq4_fixed->set_gas("CH4");' in main_cpp


def test_every_built_in_type_resolves() -> None:
    """Every table entry resolves to the coefficients the datasheet fit needs."""
    for type_key in SENSOR_TYPES:
        resolved = resolve_sensor(type_key)
        assert resolved.a != 0.0
        assert resolved.min_ppm < resolved.max_ppm


def test_mq8_h2_matches_the_reference_curve() -> None:
    """The MQ-8 H2 curve is the anchor of the component tests below."""
    resolved = resolve_sensor("MQ8", "H2")

    assert resolved.a == pytest.approx(976.97)
    assert resolved.b == pytest.approx(-0.688)
    assert resolved.method == "exponential"
    assert resolved.ratio_in_clean_air == pytest.approx(70.0)
    assert resolved.max_ppm == pytest.approx(10000.0)


def test_unknown_sensor_type_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="not a supported MQ sensor type"):
        CONFIG_SCHEMA(
            {
                "id": "mq8",
                "name": "MQ-8",
                "sensor_type": "MQ-999",
                "voltage": "mq8_adc",
            }
        )


def test_unknown_gas_for_type_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="has no curve for gas"):
        CONFIG_SCHEMA(
            {
                "id": "mq8",
                "name": "MQ-8",
                "sensor_type": "MQ-8",
                "gas": "O3",
                "voltage": "mq8_adc",
            }
        )


def test_r0_and_calibration_are_exclusive() -> None:
    """A fixed R0 and a boot calibration cannot both define the reference."""
    with pytest.raises(cv.Invalid, match="Cannot specify more than one of"):
        CONFIG_SCHEMA(
            {
                "id": "mq8",
                "name": "MQ-8",
                "sensor_type": "MQ-8",
                "voltage": "mq8_adc",
                "r0": 9.5,
                "calibration": {"delay": "5s"},
            }
        )


def test_sample_limits_follow_the_main_loop() -> None:
    """A burst longer than 16 conversions and a spacing under the loop tick are rejected."""
    base = {"id": "mq8", "name": "MQ-8", "sensor_type": "MQ-8", "voltage": "mq8_adc"}
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({**base, "sample_count": 17})
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({**base, "calibration": {"sample_interval": "10ms"}})
