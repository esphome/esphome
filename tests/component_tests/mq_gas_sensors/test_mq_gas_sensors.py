"""Tests for the MQ gas sensors code generation."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

import esphome
from esphome import config_validation as cv
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
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
from esphome.components.mq_gas_sensors.sensor import (
    _KEY_GENERATED_ADC,
    CONFIG_SCHEMA,
    _auto_load,
)
from esphome.config import read_config
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable

ESP32_PLATFORM_DATA = {KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32}
ESP8266_PLATFORM_DATA = {KEY_BOARD: "nodemcuv2"}


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
        "mq8_boot->set_regression_method(mq_gas_sensors::mqmath::REGRESSION_EXPONENTIAL);"
        in main_cpp
    )
    assert "mq8_boot->set_ratio_in_clean_air(70.0f);" in main_cpp
    assert "mq8_boot->set_voltage_multiplier(3.0f);" in main_cpp
    assert "mq8_boot->set_samples(4);" in main_cpp
    assert "mq8_boot->set_calibration(true, 70.0f, 5000, 0, 5, true);" in main_cpp
    # A fixed R0 skips the boot calibration.
    assert "mq4_fixed->set_r0(10.0f);" in main_cpp
    assert "mq4_fixed->set_calibration(" not in main_cpp
    # The primary gas is assumed when `gas:` is not written.
    assert 'mq4_fixed->set_gas("CH4");' in main_cpp


def test_every_built_in_type_resolves() -> None:
    """Every gas in the table resolves to the coefficients the datasheet fit needs."""
    for type_key, definition in SENSOR_TYPES.items():
        # A type without published curves still resolves, from the user supplied
        # `coefficient_a:` / `coefficient_b:` values.
        gases = list(definition["gases"]) or [None]
        for gas in gases:
            if requires_coefficients(type_key):
                resolved = resolve_sensor(type_key, gas, a=36.0, b=-2.0)
            else:
                resolved = resolve_sensor(type_key, gas)
            assert resolved.a != 0.0
            assert resolved.max_ppm > 0.0


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
    assert resolve_sensor("MQ3", "CH4").built_in_clamped
    assert not resolve_sensor("MQ8", "H2").built_in_clamped


def test_partial_override_still_reports_the_built_in_clamp() -> None:
    """Overriding one coefficient must not hide the built-in one being clamped."""
    resolved = resolve_sensor("MQ3", "CH4", b=10.0)
    assert resolved.a == MQ_MAX_A
    assert resolved.b == 10.0
    assert resolved.built_in_clamped

    assert not resolve_sensor("MQ3", "CH4", a=1e10, b=10.0).built_in_clamped


def test_unusable_built_in_curve_is_reported(
    set_core_config: SetCoreConfigCallable, caplog: pytest.LogCaptureFixture
) -> None:
    """MQ-3 CH4 is the one built-in fit above the firmware clamp, so it is reported."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    with caplog.at_level("WARNING"):
        CONFIG_SCHEMA(
            {
                "id": "mq3_ch4",
                "name": "MQ-3 CH4",
                "sensor_type": "MQ-3",
                "gas": "CH4",
                "voltage": "mq8_adc",
                "r0": 10.0,
            }
        )
    assert (
        "a built-in coefficient is outside the range the firmware evaluates"
        in caplog.text
    )


def test_clamped_coefficient_is_reported(
    set_core_config: SetCoreConfigCallable, caplog: pytest.LogCaptureFixture
) -> None:
    """A user supplied coefficient the device has to clamp is not silently changed."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    with caplog.at_level("WARNING"):
        CONFIG_SCHEMA(
            {
                "id": "custom",
                "name": "Custom",
                "sensor_type": "CUSTOM",
                "voltage": "mq8_adc",
                "coefficient_a": 1e40,
                "coefficient_b": 1000.0,
                "r0": 10.0,
            }
        )
    assert (
        "is outside the range the firmware evaluates and is clamped to" in caplog.text
    )
    assert f"clamped to {MQ_MAX_A:g}" in caplog.text
    assert f"clamped to {MQ_MAX_B:g}" in caplog.text


def test_non_finite_coefficient_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """`clamp_a()` turns a non-finite `a` into 0, which drops the curve."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {
        "id": "mq136",
        "name": "MQ-136",
        "sensor_type": "MQ-136",
        "voltage": "mq8_adc",
        "coefficient_a": 36.0,
        "coefficient_b": -2.0,
        "r0": 10.0,
    }
    with pytest.raises(cv.Invalid, match="must be a finite number"):
        CONFIG_SCHEMA({**base, "coefficient_a": float("nan")})
    with pytest.raises(cv.Invalid, match="must be a finite number"):
        CONFIG_SCHEMA(
            {**base, "name": "MQ-136 infinite", "coefficient_b": float("inf")}
        )


@pytest.mark.parametrize(
    "key",
    [
        "r0",
        "vcc",
        "rl",
        "ratio_in_clean_air",
        "voltage_multiplier",
        "adc_input_max",
        "adc_pin_max",
        "min_ppm",
        "max_ppm",
    ],
)
def test_non_finite_option_is_rejected(
    set_core_config: SetCoreConfigCallable, key: str
) -> None:
    """Infinity makes every derived value zero or infinite; no field may accept it."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {
        "id": "mq8",
        "name": "MQ-8",
        "sensor_type": "MQ-8",
        "voltage": "mq8_adc",
        "r0": 10.0,
    }
    with pytest.raises(cv.Invalid, match="must be a finite number"):
        CONFIG_SCHEMA({**base, key: float("inf")})


@pytest.mark.parametrize("key", ["r1", "r2"])
def test_non_finite_divider_resistor_is_rejected(
    set_core_config: SetCoreConfigCallable, key: str
) -> None:
    """An infinite resistor makes the multiplier and every reading infinite."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {
        "id": "mq8",
        "name": "MQ-8",
        "sensor_type": "MQ-8",
        "voltage": "mq8_adc",
        "r0": 10.0,
    }
    with pytest.raises(cv.Invalid, match="must be a finite number"):
        CONFIG_SCHEMA({**base, "voltage_divider": {key: float("inf")}})


def test_nan_noise_floor_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """`min_ppm` is the only signed float, so it is the only one NaN passed before."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {
        "id": "mq8",
        "name": "MQ-8",
        "sensor_type": "MQ-8",
        "voltage": "mq8_adc",
        "r0": 10.0,
    }
    with pytest.raises(cv.Invalid, match="must be a finite number"):
        CONFIG_SCHEMA({**base, "min_ppm": float("nan")})


def test_correction_factor_must_keep_the_clean_air_ratio(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """MQ-135 reads RS/R0 = 1 in clean air, so a factor of -1 hides the sensor."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {
        "id": "mq135",
        "name": "MQ-135",
        "sensor_type": "MQ-135",
        "voltage": "mq8_adc",
        "r0": 10.0,
    }
    with pytest.raises(cv.Invalid, match="clean air would no longer be"):
        CONFIG_SCHEMA({**base, "correction_factor": -1.0})
    config = CONFIG_SCHEMA({**base, "name": "MQ-135 shifted", "correction_factor": 0.5})
    assert config["correction_factor"] == 0.5


def test_correction_factor_follows_the_configured_ratio_mode(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """In `r0_rs` the firmware evaluates 1 / ratio_in_clean_air, not the datasheet RS/R0."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {
        "id": "mq8",
        "name": "MQ-8",
        "sensor_type": "MQ-8",
        "voltage": "mq8_adc",
        "r0": 10.0,
    }
    # MQ-8 reads RS/R0 = 70 in clean air, so -0.5 leaves 69.5 there and passes; the
    # same factor moves the 1/70 the inverted mode evaluates below zero and does not.
    config = CONFIG_SCHEMA({**base, "correction_factor": -0.5})
    assert config["correction_factor"] == -0.5
    inverted = {**base, "name": "MQ-8 inverted", "ratio_mode": "r0_rs"}
    with pytest.raises(cv.Invalid, match="clean air would no longer be"):
        CONFIG_SCHEMA({**inverted, "correction_factor": -0.5})
    config = CONFIG_SCHEMA(
        {**inverted, "name": "MQ-8 trimmed", "correction_factor": -0.01}
    )
    assert config["correction_factor"] == -0.01


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


def test_coefficients_the_method_cannot_use_are_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The MQ-135 linear pair has a negative `a`, which exponential cannot take."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {"id": "mq135", "name": "MQ-135", "sensor_type": "MQ-135", "voltage": "adc"}
    with pytest.raises(
        cv.Invalid, match="MQ-135 with 'regression_method: exponential' needs"
    ):
        CONFIG_SCHEMA({**base, "regression_method": "exponential"})


def test_coefficients_matching_the_overridden_method_are_accepted(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    config = CONFIG_SCHEMA(
        {
            "id": "mq135",
            "name": "MQ-135",
            "sensor_type": "MQ-135",
            "voltage": "adc",
            "regression_method": "exponential",
            "coefficient_a": 110.47,
            "coefficient_b": -2.862,
        }
    )
    assert config["regression_method"] == "exponential"


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

    assert "new(mq8_pin) mq_gas_sensors::MQGasSensor(mq8_pin_internal_adc);" in main_cpp
    # The 20k/10k divider derives the multiplier from the wiring.
    assert "mq8_pin->set_voltage_multiplier(3.0f);" in main_cpp
    # `persist:` is the last calibration argument.
    assert "mq8_pin->set_calibration(true, 70.0f, 5000, 0, 5, true);" in main_cpp


def test_owned_adc_keeps_the_widest_range_by_default(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """`adc_attenuation:` is optional; without it the owned ADC is at 12 dB."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {
        "id": "mq8",
        "name": "MQ-8",
        "sensor_type": "MQ-8",
        "pin": 34,
        "voltage_divider": {"r1": 10.0, "r2": 2.0},
        "vcc": 5.0,
    }

    config = CONFIG_SCHEMA(dict(base))
    assert config[_KEY_GENERATED_ADC]["attenuation"] == "12db"

    config = CONFIG_SCHEMA({**base, "name": "MQ-8 at 6 dB", "adc_attenuation": "6db"})
    assert config[_KEY_GENERATED_ADC]["attenuation"] == "6db"


def test_adc_options_need_the_owned_pin(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """`adc_attenuation:` / `adc_samples:` describe the internal ADC of `pin:`."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {"id": "mq8", "name": "MQ-8", "sensor_type": "MQ-8", "voltage": "mq8_adc"}

    with pytest.raises(cv.Invalid, match="configures the internal ADC"):
        CONFIG_SCHEMA({**base, "adc_attenuation": "6db"})
    with pytest.raises(cv.Invalid, match="configures the internal ADC"):
        CONFIG_SCHEMA({**base, "name": "MQ-8 sampled", "adc_samples": 4})


def test_internal_adc_burst_is_bounded(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """`adc_samples:` multiplies the burst, so the product is what is bounded."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {
        "id": "mq8",
        "name": "MQ-8",
        "sensor_type": "MQ-8",
        "pin": 34,
        "voltage_divider": {"r1": 10.0, "r2": 2.0},
    }

    # 16 x 4 is the largest burst the bound allows.
    CONFIG_SCHEMA({**base, "sample_count": 16, "adc_samples": 4})
    with pytest.raises(cv.Invalid, match="times per reading"):
        CONFIG_SCHEMA(
            {
                **base,
                "id": "mq8_too_long",
                "name": "MQ-8 long burst",
                "sample_count": 16,
                "adc_samples": 255,
            }
        )


def test_sample_count_and_interval_stay_within_one_loop_pass(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A burst longer than a loop tick (or a shorter spacing) is rejected."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {"id": "mq8", "name": "MQ-8", "sensor_type": "MQ-8", "voltage": "mq8_adc"}

    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({**base, "sample_count": 17})
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({**base, "name": "MQ-8 fast", "sample_interval": "10ms"})


def test_text_sensor_is_only_loaded_for_the_log_mirror() -> None:
    """The log mirror is opt-in, so its platform is not linked without it."""
    base = {"id": "mq8", "pin": 34}
    assert _auto_load(base) == ["sensor", "voltage_sampler", "adc"]
    assert _auto_load({**base, "log_sensor": "mq8_logs"}) == [
        "sensor",
        "voltage_sampler",
        "adc",
        "text_sensor",
    ]


def test_log_mirror_define_follows_the_option(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The C++ mirror is compiled in only when a `log_sensor:` is configured."""
    generate_main(component_config_path("mq_gas_sensors.yaml"))

    assert any(
        define.name == "USE_MQ_GAS_SENSORS_LOG_SENSOR" for define in CORE.defines
    )


def test_log_mirror_define_is_absent_without_the_option(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A configuration without `log_sensor:` must not compile the mirror in."""
    generate_main(component_config_path("no_log_sensor.yaml"))

    assert not any(
        define.name == "USE_MQ_GAS_SENSORS_LOG_SENSOR" for define in CORE.defines
    )


def test_esp32_attenuation_ranges_follow_the_variant() -> None:
    """Each variant uses its documented measuring range and `auto` the widest."""
    assert esp32_attenuation_input_max(VARIANT_ESP32, "6db") == pytest.approx(1.75)
    assert esp32_attenuation_input_max(VARIANT_ESP32S3, "12db") == pytest.approx(3.1)
    assert esp32_attenuation_input_max(VARIANT_ESP32, "11db") == pytest.approx(2.45)
    assert esp32_attenuation_input_max(VARIANT_ESP32, "auto") == pytest.approx(2.45)


def test_variant_without_a_row_uses_the_narrowest_range(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """An unverified variant must not be trusted with the classic ESP32 range."""
    with caplog.at_level("WARNING"):
        assert esp32_attenuation_input_max("ESP32P4", "12db") == pytest.approx(2.5)
    assert "ESP32P4" in caplog.text
    assert "narrowest known range" in caplog.text


def test_esp32_s31_owned_adc_uses_its_only_attenuation(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The S31 ADC has a single attenuation, so the 12 dB default cannot apply."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s31", KEY_VARIANT: VARIANT_ESP32S31},
    )
    base = {
        "id": "mq8",
        "name": "MQ-8",
        "sensor_type": "MQ-8",
        "pin": 42,
        "voltage_divider": {"r1": 10.0, "r2": 1.0},
        "vcc": 5.0,
    }

    config = CONFIG_SCHEMA(dict(base))
    assert config[_KEY_GENERATED_ADC]["attenuation"] == "0db"

    with pytest.raises(cv.Invalid, match="single attenuation"):
        CONFIG_SCHEMA({**base, "name": "MQ-8 at 12 dB", "adc_attenuation": "12db"})


def test_declared_range_above_the_owned_adc_is_capped(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A declared `adc_input_max:` cannot widen the range of an owned ADC."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    base = {
        "id": "mq8",
        "name": "MQ-8",
        "sensor_type": "MQ-8",
        "pin": 34,
        "vcc": 5.0,
        "adc_attenuation": "12db",
        "adc_input_max": 3.34,
    }
    # 5 V through the 10k/20k divider is 3.33 V: below the declared 3.34 V, above
    # the 2.45 V a 12 dB classic ESP32 measures, so the top of the reading clips.
    with (
        caplog.at_level("WARNING"),
        pytest.raises(cv.Invalid, match="above the 2.45 V"),
    ):
        CONFIG_SCHEMA({**base, "voltage_divider": {"r1": 10.0, "r2": 20.0}})
    assert "'adc_input_max: 3.34' is above the 2.45 V" in caplog.text

    # A divider that stays inside the range is accepted, and the capped
    # declaration is still reported.
    caplog.clear()
    with caplog.at_level("WARNING"):
        CONFIG_SCHEMA(
            {
                **base,
                "name": "MQ-8 inside",
                "voltage_divider": {"r1": 20.0, "r2": 10.0},
            }
        )
    assert "'adc_input_max: 3.34' is above the 2.45 V" in caplog.text


def test_pin_range_follows_the_attenuation(
    set_core_config: SetCoreConfigCallable,
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
    # ESP32 measures at 6 dB: the top of the range clips, so the wiring is refused.
    with pytest.raises(cv.Invalid, match="above the 1.75 V"):
        CONFIG_SCHEMA(dict(config))


def test_esp8266_pin_uses_the_fixed_input_range(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A0 is a 0 - 1.0 V input, so the ESP32 attenuation table does not describe it."""
    # Registers the ESP8266 pin schema, which `pin:` is validated against.
    import esphome.components.esp8266.gpio  # noqa: F401

    set_core_config(
        PlatformFramework.ESP8266_ARDUINO, platform_data=ESP8266_PLATFORM_DATA
    )
    config = {
        "id": "mq8",
        "name": "MQ-8",
        "sensor_type": "MQ-8",
        "pin": "A0",
        "voltage_divider": {"r1": 10.0, "r2": 20.0},
        "vcc": 5.0,
    }
    # 5 V through the divider is 3.33 V on A0, above the 1.0 V it measures.
    with pytest.raises(cv.Invalid, match="above the 1.00 V"):
        CONFIG_SCHEMA(dict(config))


def test_voltage_source_range_is_checked(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """`voltage:` is measured against the sampler too, like an owned `pin:` is.

    A declared `adc_pin_max:` is the tolerance of the pin, not the range the
    sampler measures, so the attenuation of the sampler is still what the divider
    is checked against.
    """
    CORE.config_path = component_config_path("voltage_source_range.yaml")
    assert read_config({}) is None
    captured = capsys.readouterr()
    assert "above the 1.75 V measuring range of the sampler" in captured.out


def test_declared_range_cannot_widen_the_sampler(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A declared `adc_input_max:` is capped at the sampler's attenuation range."""
    CORE.config_path = component_config_path("declared_range_voltage_source.yaml")
    with caplog.at_level("WARNING"):
        assert read_config({}) is None
    captured = capsys.readouterr()
    assert "above the 1.75 V measuring range of the sampler" in captured.out
    assert "'adc_input_max: 3.34' is above the 1.75 V" in caplog.text


def test_voltage_source_burst_is_bounded(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """`sample_count` multiplies the `samples:` of the sampler `voltage:` names."""
    CORE.config_path = component_config_path("burst_voltage_source.yaml")
    assert read_config({}) is None
    captured = capsys.readouterr()
    assert "510 times per reading" in captured.out
    assert "lower 'sample_count:' or the sampler's 'samples:'" in captured.out


def test_external_sampler_range_is_reported(
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A sampler that is not an internal ADC has a range the YAML has to declare."""
    CORE.config_path = component_config_path("external_voltage_sampler.yaml")
    with caplog.at_level("WARNING"):
        config = read_config({})

    assert config is not None
    assert "declare 'adc_input_max:'" in caplog.text


def test_raw_adc_source_is_refused(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """An ADC sensor that reports counts instead of volts is not a voltage source."""
    CORE.config_path = component_config_path("raw_adc_source.yaml")
    assert read_config({}) is None
    captured = capsys.readouterr()
    assert "outputs raw counts" in captured.out


def test_diagnostics_and_log_are_owned_entities(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The mirrors of the measurement chain are created by the component itself."""
    main_cpp = generate_main(component_config_path("mq_gas_sensors.yaml"))

    # The mirrors are child entities of the sensor - registered by the component
    # and marked as diagnostics, not wired to `sensor:` entries of the user.
    for entity_id, name in (
        ("mq8_ratio", "MQ-8 ratio"),
        ("mq8_rs", "MQ-8 RS"),
        ("mq8_voltage", "MQ-8 voltage"),
        ("mq8_correction", "MQ-8 T-RH correction"),
        ("mq8_r0", "MQ-8 R0"),
    ):
        assert f'App.register_sensor({entity_id}, "{name}"' in main_cpp
    assert 'App.register_text_sensor(mq8_logs, "MQ-8 logs"' in main_cpp
    assert "// category:diagnostic" in main_cpp
    assert "template__mq8_ratio" not in main_cpp

    assert "mq8_pin->set_ratio_sensor(mq8_ratio);" in main_cpp
    assert "mq8_pin->set_rs_sensor(mq8_rs);" in main_cpp
    assert "mq8_pin->set_voltage_sensor(mq8_voltage);" in main_cpp
    assert "mq8_pin->set_correction_sensor(mq8_correction);" in main_cpp
    assert "mq8_pin->set_r0_sensor(mq8_r0);" in main_cpp
    assert "mq8_pin->set_log_sensor(mq8_logs);" in main_cpp
    assert "mq8_pin->set_log_ppm(true);" in main_cpp
    # The linked T/RH sensors switch the compensation on.
    assert (
        "mq8_pin->set_correction_mode(mq_gas_sensors::mqmath::CORRECTION_MQDATASCIENCE);"
        in main_cpp
    )
    assert "mq8_pin->set_temperature_source(mq8_temperature);" in main_cpp
    assert "mq8_pin->set_humidity_source(mq8_humidity);" in main_cpp


def test_diagnostics_default_to_the_diagnostic_category(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A mirror is a diagnostic entity with a unit of its own, without being asked."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    config = CONFIG_SCHEMA(
        {
            "id": "mq8",
            "name": "MQ-8",
            "sensor_type": "MQ-8",
            "voltage": "mq8_adc",
            "rs_sensor": {"name": "MQ-8 RS"},
            "log_sensor": {"name": "MQ-8 logs"},
        }
    )

    assert config["rs_sensor"]["entity_category"] == ENTITY_CATEGORY_DIAGNOSTIC
    assert config["rs_sensor"]["unit_of_measurement"] == "kOhm"
    assert config["rs_sensor"]["accuracy_decimals"] == 3
    assert config["log_sensor"]["entity_category"] == ENTITY_CATEGORY_DIAGNOSTIC


def test_calibration_action_reaches_the_sensor(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """`mq_gas_sensors.calibrate_r0` runs the calibration from an automation."""
    main_cpp = generate_main(component_config_path("mq_gas_sensors.yaml"))

    assert "::mq8_boot->start_calibration();" in main_cpp


def test_a_bad_pin_is_reported_as_a_pin(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """`pin:` is validated before the divider guard, so the pin is what fails."""
    set_core_config(PlatformFramework.ESP32_IDF, platform_data=ESP32_PLATFORM_DATA)
    with pytest.raises(cv.Invalid, match="Cannot resolve pin name"):
        CONFIG_SCHEMA(
            {
                "id": "mq8",
                "name": "MQ-8",
                "sensor_type": "MQ-8",
                "pin": "NOT_A_PIN",
            }
        )


def test_the_firmware_clamp_matches_the_code_generation_clamp() -> None:
    """`mq_math.h` clamps `a` and `b` on the device with its own limits.

    A table value is clamped twice, once here by `resolve_sensor()` and once on the
    device by `mqmath::clamp_a()` / `clamp_b()`, so a change to one limit has to be
    mirrored in the other.
    """
    header = (
        Path(esphome.__file__).parent / "components" / "mq_gas_sensors" / "mq_math.h"
    )
    source = header.read_text(encoding="utf-8")

    for name, limit in (("MQ_MAX_A", MQ_MAX_A), ("MQ_MAX_B", MQ_MAX_B)):
        match = re.search(rf"static constexpr double {name} = ([^;]+);", source)
        assert match is not None, f"{name} is not defined in {header.name}"
        assert float(match.group(1)) == limit


def test_mqdatascience_curve_reaches_the_sensor(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The alternative dataset replaces the built-in coefficients."""
    main_cpp = generate_main(component_config_path("mq_gas_sensors.yaml"))

    assert "mq8_curve->set_a(18391.5667f);" in main_cpp
    assert "mq8_curve->set_b(-1.4494f);" in main_cpp
    assert (
        "mq8_curve->set_regression_method(mq_gas_sensors::mqmath::REGRESSION_INVERSE);"
        in main_cpp
    )
    assert "mq8_curve->set_max_ppm(20000.0f);" in main_cpp
