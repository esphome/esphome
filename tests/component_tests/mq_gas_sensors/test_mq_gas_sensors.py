"""Tests for the MQ gas sensors code generation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)
from esphome.components.mq_gas_sensors import esp32_attenuation_input_max
from esphome.components.mq_gas_sensors.coefficients import (
    CUSTOM_TYPE,
    MQ_MAX_A,
    MQ_MAX_B,
    SENSOR_TYPES,
    requires_coefficients,
    resolve_sensor,
)
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
    assert "mq8_boot->set_regression_method(1);" in main_cpp
    assert "mq8_boot->set_ratio_in_clean_air(70.0f);" in main_cpp
    assert "mq8_boot->set_voltage_multiplier(1.5f);" in main_cpp
    assert "mq8_boot->set_samples(4);" in main_cpp
    assert "mq8_boot->set_calibration(true, 70.0f, 5000, 0, 5, true);" in main_cpp
    # A fixed R0 skips the boot calibration.
    assert "mq4_fixed->set_r0(10.0f);" in main_cpp
    assert "mq4_fixed->set_calibration(" not in main_cpp
    # The primary gas is assumed when `gas:` is not written.
    assert 'mq4_fixed->set_gas("CH4");' in main_cpp


def test_every_built_in_type_resolves() -> None:
    """Every table entry resolves to the coefficients the datasheet fit needs."""
    for type_key in SENSOR_TYPES:
        if requires_coefficients(type_key):
            resolved = resolve_sensor(type_key, a=36.0, b=-2.0)
        else:
            resolved = resolve_sensor(type_key)
        assert resolved.a != 0.0
        assert resolved.min_ppm < resolved.max_ppm


def test_every_curve_stays_within_the_firmware_clamp() -> None:
    """The device clamps `a` / `b`, so no resolved curve may promise more than that."""
    for type_key, definition in SENSOR_TYPES.items():
        for gas in definition["gases"]:
            resolved = resolve_sensor(type_key, gas)
            assert abs(resolved.a) <= MQ_MAX_A
            assert abs(resolved.b) <= MQ_MAX_B


def test_coefficients_the_firmware_clamps_are_resolved_to_the_clamp() -> None:
    """A user supplied coefficient above `MQ_MAX_A` is clamped, not silently dropped."""
    resolved = resolve_sensor(CUSTOM_TYPE, a=1e40, b=1000.0)
    assert resolved.a == MQ_MAX_A
    assert resolved.b == MQ_MAX_B


def test_published_curve_above_the_clamp_resolves_to_the_clamp() -> None:
    """MQ-3 CH4 publishes a = 2e31, above what the firmware can evaluate."""
    assert SENSOR_TYPES["MQ3"]["gases"]["CH4"][0] == 2e31
    assert resolve_sensor("MQ3", "CH4").a == MQ_MAX_A


def test_mq8_h2_matches_the_reference_curve() -> None:
    """The MQ-8 H2 curve is the anchor of the component tests below."""
    resolved = resolve_sensor("MQ8", "H2")

    assert resolved.a == pytest.approx(976.97)
    assert resolved.b == pytest.approx(-0.688)
    assert resolved.method == "exponential"
    assert resolved.ratio_in_clean_air == pytest.approx(70.0)
    assert resolved.max_ppm == pytest.approx(10000.0)


def test_unknown_sensor_type_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    with pytest.raises(cv.Invalid, match="not a supported MQ sensor type"):
        CONFIG_SCHEMA(
            {
                "id": "mq8",
                "name": "MQ-8",
                "sensor_type": "MQ-999",
                "voltage": "mq8_adc",
            }
        )


def test_unknown_gas_for_type_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
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


def test_r0_and_calibration_warn_but_resolve(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A fixed R0 wins over the calibration; the configuration stays valid."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    config = CONFIG_SCHEMA(
        {
            "id": "mq8",
            "name": "MQ-8",
            "sensor_type": "MQ-8",
            "voltage": "mq8_adc",
            "r0": 9.5,
            "calibration": {"delay": "5s"},
        }
    )

    assert config["r0"] == pytest.approx(9.5)


def test_pin_creates_an_internal_adc(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """`pin:` owns its sampler: the hidden ADC feeds the sensor constructor."""
    main_cpp = generate_main(component_config_path("mq_gas_sensors.yaml"))

    assert "new(mq8_pin) mq_gas_sensors::MQGasSensor(mq8_pin_adc);" in main_cpp
    # The 10k/20k divider derives the multiplier from the wiring.
    assert "mq8_pin->set_voltage_multiplier(1.5f);" in main_cpp
    # `persist:` is the last calibration argument.
    assert "mq8_pin->set_calibration(true, 70.0f, 5000, 0, 5, true);" in main_cpp


def test_esp32_attenuation_ranges_follow_the_variant() -> None:
    """Each variant uses its documented measuring range and `auto` the widest."""
    assert esp32_attenuation_input_max(VARIANT_ESP32, "6db") == pytest.approx(1.75)
    assert esp32_attenuation_input_max(VARIANT_ESP32S3, "12db") == pytest.approx(3.1)
    assert esp32_attenuation_input_max(VARIANT_ESP32, "11db") == pytest.approx(2.45)
    assert esp32_attenuation_input_max(VARIANT_ESP32, "auto") == pytest.approx(2.45)
    # A variant without its own row falls back to the classic ESP32 range.
    assert esp32_attenuation_input_max("ESP32P4", "12db") == pytest.approx(2.45)


def test_pin_range_follows_the_attenuation(
    set_core_config: SetCoreConfigCallable, caplog: pytest.LogCaptureFixture
) -> None:
    """The divider guard measures against the range of the configured attenuation."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    config = {
        "id": "mq8",
        "name": "MQ-8",
        "sensor_type": "MQ-8",
        "pin": 34,
        "voltage_divider": {"r1": 10.0, "r2": 20.0},
        "vcc": 5.0,
        "adc_attenuation": "6db",
    }
    # 5 V through the divider is 3.33 V on the pin, above the 1.75 V a classic
    # ESP32 measures at 6 dB: the top of the range clips.
    with caplog.at_level("WARNING"):
        CONFIG_SCHEMA(dict(config))
    assert "above 'adc_input_max' (1.75 V)" in caplog.text


def test_diagnostics_and_log_are_wired(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("mq_gas_sensors.yaml"))

    assert "mq8_pin->set_ratio_sensor(mq8_ratio);" in main_cpp
    assert "mq8_pin->set_rs_sensor(mq8_rs);" in main_cpp
    assert "mq8_pin->set_voltage_sensor(mq8_voltage);" in main_cpp
    assert "mq8_pin->set_correction_sensor(mq8_correction);" in main_cpp
    assert "mq8_pin->set_log_sensor(mq8_logs);" in main_cpp
    assert "mq8_pin->set_log_ppm(true);" in main_cpp
    # The linked T/RH sensors switch the compensation on.
    assert "mq8_pin->set_correction_mode(1);" in main_cpp
    assert "mq8_pin->set_temperature_source(mq8_temperature);" in main_cpp
    assert "mq8_pin->set_humidity_source(mq8_humidity);" in main_cpp


def test_mqdatascience_curve_reaches_the_sensor(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The alternative dataset replaces the built-in coefficients."""
    main_cpp = generate_main(component_config_path("mq_gas_sensors.yaml"))

    assert "mq8_curve->set_a(18391.5667f);" in main_cpp
    assert "mq8_curve->set_b(-1.4494f);" in main_cpp
    assert "mq8_curve->set_regression_method(3);" in main_cpp
    assert "mq8_curve->set_max_ppm(20000.0f);" in main_cpp
