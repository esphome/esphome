"""Tests for the MQ gas sensors code generation."""

from collections.abc import Callable
import math
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.components.mq_gas_sensors.coefficients import SENSOR_TYPES, resolve_sensor
from esphome.components.mq_gas_sensors.sensor import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable

ESP32_PLATFORM_DATA = {KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32}


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
    assert (
        "mq8_boot->set_regression_method(mq_gas_sensors::RegressionMethod::REGRESSION_METHOD_EXPONENTIAL);"
        in main_cpp
    )
    assert "mq8_boot->set_ratio_in_clean_air(70.0f);" in main_cpp
    assert "mq8_boot->set_voltage_multiplier(1.5f);" in main_cpp
    assert "mq8_boot->set_samples(4);" in main_cpp
    assert "mq8_boot->set_r0(10.0f);" in main_cpp
    assert "mq4_fixed->set_r0(10.0f);" in main_cpp
    # The primary gas is assumed when `gas:` is not written.
    assert 'mq4_fixed->set_gas("CH4");' in main_cpp


def test_every_built_in_type_resolves() -> None:
    """Every gas in the table resolves to the coefficients the datasheet fit needs."""
    for type_key, definition in SENSOR_TYPES.items():
        for gas in definition["gases"]:
            resolved = resolve_sensor(type_key, gas)
            assert resolved.a != 0.0
            assert resolved.max_ppm > 0.0


def test_mq3_has_no_ch4_curve() -> None:
    """The collected MQ-3 CH4 fit is unusable, so the combination fails the build."""
    assert "CH4" not in SENSOR_TYPES["MQ3"]["gases"]
    with pytest.raises(ValueError, match="MQ-3 has no curve for gas 'CH4'"):
        resolve_sensor("MQ3", "CH4")


def test_mq2_has_no_duplicate_ch4_curve() -> None:
    """The MQ-2 CH4 row repeated the CO coefficients, so the combination fails the build."""
    assert "CH4" not in SENSOR_TYPES["MQ2"]["gases"]
    with pytest.raises(ValueError, match="MQ-2 has no curve for gas 'CH4'"):
        resolve_sensor("MQ2", "CH4")


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
                "r0": 10.0,
            }
        )


def test_r0_is_required() -> None:
    """Without the clean-air reference the sensor cannot produce a PPM value."""
    with pytest.raises(cv.Invalid, match="required key not provided"):
        CONFIG_SCHEMA(
            {
                "id": "mq8",
                "name": "MQ-8",
                "sensor_type": "MQ-8",
                "voltage": "mq8_adc",
            }
        )


def test_empty_gas_is_rejected() -> None:
    """An empty `gas:` must not quietly select the default gas of the type."""
    with pytest.raises(cv.Invalid, match="gas must not be empty"):
        CONFIG_SCHEMA(
            {
                "id": "mq8",
                "name": "MQ-8",
                "sensor_type": "MQ-8",
                "gas": "",
                "voltage": "mq8_adc",
                "r0": 10.0,
            }
        )


def test_published_clean_air_ratios() -> None:
    """A placeholder ratio would make the datasheet values wrong."""
    assert resolve_sensor("MQ131", "O3").ratio_in_clean_air == pytest.approx(1.0)
    assert resolve_sensor("MQ135", "NH3").ratio_in_clean_air == pytest.approx(3.6)
    # No published value: the type has no clean-air anchor of its own.
    assert resolve_sensor("MQ137", "NH3").ratio_in_clean_air == 0.0
    assert resolve_sensor("MQ138", "TOLUENE").ratio_in_clean_air == 0.0


def test_sample_limits_follow_the_main_loop() -> None:
    """A burst longer than 16 conversions is rejected."""
    base = {"id": "mq8", "name": "MQ-8", "sensor_type": "MQ-8", "voltage": "mq8_adc"}
    with pytest.raises(cv.Invalid, match="at most 16"):
        CONFIG_SCHEMA({**base, "r0": 10.0, "sample_count": 17})


def test_mq131_ozone_rises_with_the_sensor_resistance() -> None:
    """RS of the MQ-131 grows with ozone, and the range is 0.01 - 1 ppm."""
    resolved = resolve_sensor("MQ131", "O3")
    assert resolved.method == "linear"
    assert resolved.max_ppm == pytest.approx(1.0)

    def ppm(ratio: float) -> float:
        return 10 ** ((math.log10(ratio) - resolved.b) / resolved.a)

    # The reference fit gives ppb; clean air is near the bottom of the range.
    assert ppm(1.0) == pytest.approx(10 ** (0.4708 / 0.41195) / 1000, rel=1e-4)
    assert 0.01 < ppm(1.0) < ppm(3.0) < 1.0


@pytest.mark.parametrize(
    ("gas", "mg_per_l_a", "molar_mass"),
    [
        ("ALCOHOL", 0.3934, 46.07),
        ("BENZENE", 4.8387, 78.11),
        ("CO", 521853.0, 28.01),
        ("HEXANE", 7585.3, 86.18),
        ("LPG", 44771.0, 51.11),
    ],
)
def test_mq3_curves_are_converted_to_ppm(
    gas: str, mg_per_l_a: float, molar_mass: float
) -> None:
    """The MQ-3 fits give mg/L: every gas has to be scaled to ppm."""
    resolved = resolve_sensor("MQ3", gas)

    assert resolved.a == pytest.approx(mg_per_l_a * 24450 / molar_mass)


@pytest.mark.parametrize(("sensor_type", "expected"), [("MQ-131", 3), ("MQ-8", 1)])
def test_default_accuracy_follows_the_range(
    set_core_config: SetCoreConfigCallable, sensor_type: str, expected: int
) -> None:
    """One decimal cannot show a range that ends at 1 ppm."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {
        "id": "mq",
        "name": "MQ",
        "sensor_type": sensor_type,
        "voltage": "mq8_adc",
        "r0": 10.0,
    }

    assert CONFIG_SCHEMA(dict(base))["accuracy_decimals"] == expected
    config = CONFIG_SCHEMA({**base, "name": "MQ set", "accuracy_decimals": 2})
    assert config["accuracy_decimals"] == 2
