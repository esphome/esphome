"""Tests for the MiCS-5524 gas sensor code generation."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.components.mics_5524.coefficients import (
    GASES,
    DatasheetCurve,
    GasDefinition,
    VendorCurve,
    datasheet_gases,
)
from esphome.components.mics_5524.sensor import CONFIG_SCHEMA, _voltage_multiplier
from esphome.const import (
    CONF_AMMONIA,
    CONF_CARBON_MONOXIDE,
    CONF_ETHANOL,
    CONF_HYDROGEN,
    CONF_METHANE,
    PlatformFramework,
)
from esphome.helpers import fnv1_hash
from tests.component_tests.types import SetCoreConfigCallable


def _gas_arguments(main_cpp: str, variable: str) -> list[str]:
    """Arguments of the constructor call that creates one gas sub-sensor."""
    match = re.search(
        rf"new\({variable}\) mics_5524::MiCS5524GasSensor\((.*?)\);",
        main_cpp,
    )
    assert match is not None, f"{variable} was not constructed"
    return [argument.strip() for argument in match.group(1).split(",")]


def _gas_constants(arguments: list[str]) -> list[float]:
    """The numeric arguments of a constructor call, read as floats.

    Comparing numbers rather than their text keeps the test about the values and not
    about how code generation happens to format them.
    """
    return [float(argument.removesuffix("f")) for argument in arguments]


def test_voltage_source_is_a_constructor_argument(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Every reading needs the sampler, so it is passed at construction."""
    main_cpp = generate_main(component_config_path("mics_5524.yaml"))

    assert (
        "new(mics_5524_boot) mics_5524::MiCS5524Component(mics_5524_adc);" in main_cpp
    )
    assert (
        "new(mics_5524_fixed) mics_5524::MiCS5524Component(mics_5524_adc);" in main_cpp
    )
    assert "set_source(" not in main_cpp


def test_every_gas_becomes_a_sub_sensor_of_its_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The constants are passed to the constructor and the sub-sensor to its own hub."""
    main_cpp = generate_main(component_config_path("mics_5524.yaml"))

    # The vendor threshold and gain and the measuring range; the model lives on the hub.
    co = _gas_arguments(main_cpp, "mics_5524_co")
    assert _gas_constants(co) == [0.425, 0.000405, 1.0, 500.0]
    assert "mics_5524_boot->register_gas_sensor(mics_5524_co);" in main_cpp

    # A gas without `max_ppm:` keeps the measuring range of the vendor.
    ch4 = _gas_arguments(main_cpp, "mics_5524_ch4")
    assert _gas_constants(ch4) == [0.786, 0.000023, 1000.0, 25000.0]
    assert "mics_5524_boot->register_gas_sensor(mics_5524_ch4);" in main_cpp

    # The gas belongs to the sensor it is configured under, not to the first one.
    assert "mics_5524_fixed->register_gas_sensor(mics_5524_h2);" in main_cpp
    assert "mics_5524_boot->register_gas_sensor(mics_5524_h2);" not in main_cpp


def test_shared_options_reach_the_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The analog front end is configured once per physical sensor."""
    main_cpp = generate_main(component_config_path("mics_5524.yaml"))

    assert (
        "mics_5524_boot->set_conversion(mics_5524::micsmath::CONVERSION_MODEL_DFROBOT);"
        in main_cpp
    )
    assert "mics_5524_boot->set_vcc(5.0f);" in main_cpp
    # `rl:` is not configured, so the C++ default stands.
    assert "mics_5524_boot->set_rl(" not in main_cpp
    assert "mics_5524_boot->set_voltage_multiplier(1.5f);" in main_cpp
    assert "mics_5524_boot->set_samples(3);" in main_cpp
    assert "mics_5524_boot->set_warmup_time(180000);" in main_cpp
    # The stored reference is keyed by the component ID and the analog front end, so a
    # new vcc, scaling or rl never loads a value captured with the old one.
    assert (
        f"mics_5524_boot->set_preference_key({fnv1_hash('mics_5524_boot:5:1.5')}UL);"
        in main_cpp
    )
    assert (
        f"mics_5524_pin->set_preference_key({fnv1_hash('mics_5524_pin:5:1.5')}UL);"
        in main_cpp
    )
    assert "mics_5524_boot->set_calibration(5000, 3, 20, true, true);" in main_cpp
    assert "mics_5524_fixed->set_reference(3.6f);" in main_cpp
    # `air_reference:` and `calibration:` exclude each other, so the sensor with the
    # hard-coded reference only gets the defaults for a requested calibration.
    assert "mics_5524_fixed->set_calibration(60000, 10, 20, false, false);" in main_cpp


def test_datasheet_conversion_uses_the_shipped_coefficients(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """One conversion model per sensor, and the coefficients of CO are the shipped ones."""
    main_cpp = generate_main(component_config_path("mics_5524.yaml"))

    # a = 1.0, b = -1.2, the 1 ppm floor of the data sheet and the configured 2000 ppm ceiling.
    co = _gas_arguments(main_cpp, "mics_5524_co_ds")
    assert _gas_constants(co) == [1.0, -1.2, 1.0, 2000.0]
    assert "mics_5524_pin->register_gas_sensor(mics_5524_co_ds);" in main_cpp
    assert (
        "mics_5524_pin->set_conversion(mics_5524::micsmath::CONVERSION_MODEL_DATASHEET);"
        in main_cpp
    )
    assert "mics_5524_pin->set_reference(10.0f);" in main_cpp


def test_diagnostics_reach_the_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The ratio, RS and voltage of every reading can be published to their own sensor."""
    main_cpp = generate_main(component_config_path("mics_5524.yaml"))

    assert "mics_5524_pin->set_ratio_sensor(mics_5524_ratio);" in main_cpp
    assert "mics_5524_pin->set_rs_sensor(mics_5524_rs);" in main_cpp
    assert "mics_5524_pin->set_voltage_sensor(mics_5524_volts);" in main_cpp


def test_pin_creates_an_internal_adc(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A `pin:` configuration owns its ADC, and the divider is part of the reading."""
    main_cpp = generate_main(component_config_path("mics_5524.yaml"))

    assert "new(mics_5524_pin_adc) adc::ADCSensor();" in main_cpp
    # Without `adc_attenuation:` an ESP32 gets 12 dB, since the 0 dB default clips at about 1 V.
    assert (
        "mics_5524_pin_adc->set_attenuation(adc::ADC_ATTEN_DB_12_COMPAT);" in main_cpp
    )
    assert (
        "new(mics_5524_pin) mics_5524::MiCS5524Component(mics_5524_pin_adc);"
        in main_cpp
    )
    # 10k/20k under the 5 V of the breakout: 3.33 V on the pin, x1.5 in the reading.
    assert "mics_5524_pin->set_voltage_multiplier(1.5f);" in main_cpp


def _hub(air_reference: float | None = 3.6, **overrides: object) -> dict:
    """A minimal valid hub configuration with one hydrogen sub-sensor."""
    config: dict = {
        "id": "mics_5524",
        "voltage": "mics_5524_adc",
        "vcc": 5.0,
        "hydrogen": {"name": "MiCS-5524 H2"},
    }
    if air_reference is not None:
        config["air_reference"] = air_reference
    for key, value in overrides.items():
        if value is None:
            config.pop(key, None)
        else:
            config[key] = value
    return config


def test_air_reference_and_calibration_exclude_each_other() -> None:
    """Two references for one sensor would silently pick one, so it is an error."""
    with pytest.raises(cv.Invalid, match="Cannot specify more than one of"):
        CONFIG_SCHEMA(_hub(calibration={"delay": "5s"}))


@pytest.mark.parametrize("air_reference", [5.0, 6.0])
def test_air_reference_at_or_above_vcc_is_rejected(air_reference: float) -> None:
    with pytest.raises(cv.Invalid, match="must be below vcc"):
        CONFIG_SCHEMA(_hub(air_reference=air_reference))


def test_air_reference_below_vcc_is_accepted() -> None:
    assert CONFIG_SCHEMA(_hub())["air_reference"] == 3.6


def test_at_least_one_gas_is_required() -> None:
    """A sensor without a gas has nothing to publish."""
    with pytest.raises(cv.Invalid, match="Must contain at least one of"):
        CONFIG_SCHEMA(
            {
                "id": "mics_5524",
                "voltage": "mics_5524_adc",
                "vcc": 5.0,
                "air_reference": 3.6,
            }
        )


def test_the_sampler_is_given_or_owned() -> None:
    """One source only: an existing sampler, or a pin the component reads itself."""
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(
            {"id": "mics_5524", "vcc": 5.0, "hydrogen": {}, "air_reference": 3.6}
        )

    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_hub(pin=34))


def test_max_ppm_above_the_vendor_range_is_rejected() -> None:
    """A ceiling the conversion can never reach is a configuration error."""
    with pytest.raises(cv.Invalid, match="above the CO measuring range"):
        CONFIG_SCHEMA(
            _hub(carbon_monoxide={"name": "MiCS-5524 CO (rejected)", "max_ppm": 2000.0})
        )


def test_max_ppm_at_the_vendor_range_is_accepted() -> None:
    config = CONFIG_SCHEMA(
        _hub(carbon_monoxide={"name": "MiCS-5524 CO (accepted)", "max_ppm": 1000.0})
    )

    assert config["carbon_monoxide"]["max_ppm"] == 1000.0


def test_max_ppm_defaults_to_the_vendor_range_of_the_gas(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """An omitted ceiling still reports the vendor range: it is resolved per model."""
    from esphome.components.mics_5524.coefficients import resolve_gas

    resolved = resolve_gas("carbon_monoxide", "dfrobot", max_ppm=None)
    assert resolved.max_ppm == 1000.0

    # End to end: CH4 of the boot sensor has no max_ppm: and keeps 25000 ppm.
    main_cpp = generate_main(component_config_path("mics_5524.yaml"))
    ch4 = _gas_arguments(main_cpp, "mics_5524_ch4")
    assert _gas_constants(ch4)[-1] == 25000.0


def test_max_ppm_must_stay_above_min_ppm() -> None:
    with pytest.raises(cv.Invalid, match="must be greater than min_ppm"):
        CONFIG_SCHEMA(
            _hub(
                carbon_monoxide={
                    "name": "MiCS-5524 CO (narrow)",
                    "min_ppm": 100.0,
                    "max_ppm": 50.0,
                }
            )
        )


def test_calibration_cannot_sample_faster_than_the_loop() -> None:
    """The main loop runs every ~16 ms, so a shorter spacing samples nothing extra."""
    with pytest.raises(cv.Invalid, match="value must be at least 16ms"):
        CONFIG_SCHEMA(
            {
                "id": "mics_5524",
                "voltage": "mics_5524_adc",
                "vcc": 5.0,
                "hydrogen": {"name": "MiCS-5524 H2 (tick)"},
                "calibration": {"sample_interval": "10ms"},
            }
        )


def test_voltage_multiplier_defaults_to_one() -> None:
    config = CONFIG_SCHEMA(_hub())

    assert _voltage_multiplier(config) == 1.0


def test_voltage_divider_derives_the_multiplier(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The divider the user wired is turned into the multiplier of every reading."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = CONFIG_SCHEMA(
        _hub(voltage_divider={"r1": 10000.0, "r2": 20000.0}, adc_input_max=3.34)
    )

    assert _voltage_multiplier(config) == 1.5


def test_voltage_divider_rejects_an_explicit_multiplier(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The divider already says what the multiplier is, so the two would contradict."""
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match="Cannot specify more than one of"):
        CONFIG_SCHEMA(
            _hub(
                voltage_divider={"r1": 10000.0, "r2": 20000.0},
                voltage_multiplier=1.5,
                adc_input_max=3.34,
            )
        )


def test_voltage_divider_above_the_adc_limit_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A 10k/20k divider on a 5 V sensor puts 3.33 V on a classic ESP32 ADC."""
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match="above the 3.3 V measuring range"):
        CONFIG_SCHEMA(_hub(voltage_divider={"r1": 10000.0, "r2": 20000.0}))


def test_pin_source_must_be_scaled_into_the_adc_range(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A 5 V sensor straight on an ESP32 pin clips; a divider or multiplier that fits passes."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    with pytest.raises(cv.Invalid, match="5 V supply reaches the pin unscaled"):
        CONFIG_SCHEMA(_hub(voltage=None, pin=34))
    # Each hub needs its own entity names
    CONFIG_SCHEMA(
        _hub(
            voltage=None,
            pin=34,
            voltage_multiplier=1.5,
            adc_input_max=3.34,
            hydrogen={"name": "H2 behind a multiplier"},
        )
    )
    CONFIG_SCHEMA(
        _hub(
            voltage=None,
            pin=34,
            voltage_divider={"r1": 10000.0, "r2": 20000.0},
            adc_input_max=3.34,
            hydrogen={"name": "H2 behind a divider"},
        )
    )


def test_pin_source_range_follows_the_attenuation(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """At 6 dB a classic ESP32 measures up to 1.75 V, so 3.33 V on the pin is rejected."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    with pytest.raises(cv.Invalid, match="above the 1.75 V measuring range"):
        CONFIG_SCHEMA(
            _hub(voltage=None, pin=34, voltage_multiplier=1.5, adc_attenuation="6db")
        )
    CONFIG_SCHEMA(
        _hub(
            voltage=None,
            pin=34,
            voltage_multiplier=3.0,
            adc_attenuation="6db",
            hydrogen={"name": "H2 at 6 dB"},
        )
    )
    # auto picks the widest range, 2.45 V on a classic ESP32
    with pytest.raises(cv.Invalid, match="above the 2.45 V measuring range"):
        CONFIG_SCHEMA(
            _hub(
                voltage=None,
                pin=34,
                voltage_multiplier=1.5,
                adc_attenuation="auto",
                hydrogen={"name": "H2 auto"},
            )
        )


def test_min_ppm_zero_disables_the_floor() -> None:
    config = CONFIG_SCHEMA(_hub(hydrogen={"name": "MiCS-5524 H2", "min_ppm": 0.0}))
    assert config["hydrogen"]["min_ppm"] == 0.0


def test_voltage_divider_limits_follow_the_esp8266_adc(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A bare ESP8266 A0 reads 0 - 1 V, so the ESP32 limits must not apply there."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid, match="above the 1 V"):
        CONFIG_SCHEMA(_hub(voltage_divider={"r1": 10000.0, "r2": 20000.0}))
    config = CONFIG_SCHEMA(
        _hub(
            voltage_divider={"r1": 10000.0, "r2": 20000.0},
            adc_input_max=3.34,
            adc_pin_max=3.6,
            hydrogen={"name": "MiCS-5524 H2 (declared limits)"},
        )
    )
    assert config["voltage_divider"]["r1"] == 10000.0


def test_voltage_divider_below_the_declared_limit_is_accepted(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A sampler with a wider range declares it, and the same divider then passes."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = CONFIG_SCHEMA(
        _hub(
            voltage_divider={"r1": 10000.0, "r2": 20000.0},
            adc_input_max=3.34,
            adc_pin_max=3.4,
        )
    )

    assert config["voltage_divider"]["r1"] == 10000.0


def test_voltage_divider_on_an_unknown_platform_needs_declared_limits(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """No built-in ADC limits exist for LibreTiny, so the guard cannot run without them."""
    set_core_config(PlatformFramework.BK72XX_ARDUINO)
    with pytest.raises(cv.Invalid, match="no built-in ADC limits for bk72xx"):
        CONFIG_SCHEMA(_hub(voltage_divider={"r1": 10000.0, "r2": 20000.0}))
    config = CONFIG_SCHEMA(
        _hub(
            voltage_divider={"r1": 10000.0, "r2": 20000.0},
            adc_input_max=3.34,
            adc_pin_max=3.6,
            hydrogen={"name": "MiCS-5524 H2 (libretiny)"},
        )
    )
    assert config["voltage_divider"]["r1"] == 10000.0


@pytest.mark.parametrize("key", ["adc_input_max", "adc_pin_max"])
def test_adc_limits_without_a_divider_are_rejected(key: str) -> None:
    """The limits only check a divider, so without one they would be silently ignored."""
    with pytest.raises(
        cv.Invalid,
        match=f"'{key}:' only checks a 'pin:' source or a 'voltage_divider:'",
    ):
        CONFIG_SCHEMA(_hub(**{key: 3.34}))


def test_adc_attenuation_needs_an_owned_pin() -> None:
    """The attenuation configures the hidden ADC of pin:, not an external sampler."""
    with pytest.raises(
        cv.Invalid, match="'adc_attenuation:' only applies to the ADC of"
    ):
        CONFIG_SCHEMA(_hub(adc_attenuation="12db"))


def test_datasheet_conversion_is_not_capped_by_the_vendor_range() -> None:
    """The datasheet fit has no published range, so a wider one may be configured."""
    config = CONFIG_SCHEMA(
        _hub(
            air_reference=None,
            conversion="datasheet",
            r0=10.0,
            hydrogen=None,
            carbon_monoxide={"name": "MiCS-5524 CO (datasheet)", "max_ppm": 2000.0},
        )
    )

    assert config["carbon_monoxide"]["max_ppm"] == 2000.0


def test_datasheet_needs_coefficients_for_gases_without_a_shipped_fit() -> None:
    with pytest.raises(
        cv.Invalid, match="no built-in 'conversion: datasheet' coefficients"
    ):
        CONFIG_SCHEMA(_hub(air_reference=None, conversion="datasheet", r0=10.0))


def test_datasheet_accepts_explicit_coefficients() -> None:
    config = CONFIG_SCHEMA(
        _hub(
            air_reference=None,
            conversion="datasheet",
            r0=10.0,
            hydrogen={
                "name": "MiCS-5524 H2 (fit)",
                "coefficient_a": 12.0,
                "coefficient_b": -0.8,
            },
        )
    )

    assert config["hydrogen"]["coefficient_a"] == 12.0


def test_coefficients_are_rejected_by_the_vendor_model() -> None:
    # A pair under the vendor model is rejected by the model check ...
    with pytest.raises(cv.Invalid, match="remove them or switch the conversion"):
        CONFIG_SCHEMA(
            _hub(
                hydrogen={
                    "name": "MiCS-5524 H2 (stray)",
                    "coefficient_a": 12.0,
                    "coefficient_b": -0.8,
                }
            )
        )
    # ... while half a pair is already rejected by the schema itself.
    with pytest.raises(cv.Invalid, match="coefficients"):
        CONFIG_SCHEMA(
            _hub(hydrogen={"name": "MiCS-5524 H2 (half)", "coefficient_a": 12.0})
        )


def test_datasheet_rejects_half_a_fit() -> None:
    """One coefficient with the shipped value of the other mixes two fits."""
    with pytest.raises(cv.Invalid, match="coefficients"):
        CONFIG_SCHEMA(
            _hub(
                air_reference=None,
                conversion="datasheet",
                r0=10.0,
                hydrogen=None,
                carbon_monoxide={
                    "name": "MiCS-5524 CO (half)",
                    "coefficient_a": 12.0,
                },
            )
        )


def test_datasheet_rejects_a_flat_or_rising_fit() -> None:
    """A fit whose reading does not rise as the resistance falls reads saturation as no gas."""
    for b in (0.0, 0.8):
        with pytest.raises(cv.Invalid, match="'coefficient_b:' must be negative"):
            CONFIG_SCHEMA(
                _hub(
                    air_reference=None,
                    conversion="datasheet",
                    r0=10.0,
                    hydrogen={
                        "name": f"MiCS-5524 H2 (fit b={b})",
                        "coefficient_a": 12.0,
                        "coefficient_b": b,
                    },
                )
            )


def test_datasheet_min_ppm_defaults_to_the_shipped_floor_only() -> None:
    """The shipped CO fit reads 0 below the 1 ppm of the data sheet; a user fit has no floor."""
    from esphome.components.mics_5524.coefficients import resolve_gas

    assert resolve_gas("carbon_monoxide", "datasheet").min_ppm == 1.0
    assert resolve_gas("carbon_monoxide", "datasheet", a=2.0, b=-1.0).min_ppm == 0.0
    assert resolve_gas("carbon_monoxide", "datasheet", min_ppm=5.0).min_ppm == 5.0


def test_datasheet_rejects_a_non_positive_coefficient() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(
            _hub(
                air_reference=None,
                conversion="datasheet",
                r0=10.0,
                hydrogen={
                    "name": "MiCS-5524 H2 (bad fit)",
                    "coefficient_a": 0.0,
                    "coefficient_b": -0.8,
                },
            )
        )


def test_rs_sensor_is_rejected_by_the_vendor_model() -> None:
    """RS only exists in the datasheet model, so the vendor one cannot publish it."""
    with pytest.raises(cv.Invalid, match="'rs_sensor:' belongs to"):
        CONFIG_SCHEMA(_hub(rs_sensor={"name": "MiCS-5524 RS"}))


def test_reference_keys_belong_to_one_model() -> None:
    with pytest.raises(cv.Invalid, match="'r0:' belongs to 'conversion: datasheet'"):
        CONFIG_SCHEMA(_hub(air_reference=None, r0=10.0))

    with pytest.raises(
        cv.Invalid, match="'air_reference:' belongs to 'conversion: dfrobot'"
    ):
        CONFIG_SCHEMA(
            _hub(
                air_reference=3.6,
                conversion="datasheet",
                hydrogen={"name": "MiCS-5524 H2 (wrong model)"},
            )
        )


def test_vendor_table_is_the_datasheet_one() -> None:
    """The whole table is pinned, not only the gases a config file happens to use."""
    expected = {
        CONF_CARBON_MONOXIDE: GasDefinition(
            vendor=VendorCurve("CO", 0.425, 0.000405, 1.0, 1000.0),
            datasheet=DatasheetCurve(1.0, -1.2, 1.0, 1000.0),
        ),
        CONF_HYDROGEN: GasDefinition(
            VendorCurve("H2", 0.279, 0.00026, 1.0, 1000.0), None
        ),
        CONF_ETHANOL: GasDefinition(
            VendorCurve("C2H5OH", 0.306, 0.00057, 10.0, 500.0), None
        ),
        CONF_AMMONIA: GasDefinition(VendorCurve("NH3", 0.8, 0.0015, 10.0, 500.0), None),
        CONF_METHANE: GasDefinition(
            VendorCurve("CH4", 0.786, 0.000023, 1000.0, 25000.0), None
        ),
    }

    assert expected == GASES


def test_only_co_ships_with_datasheet_coefficients() -> None:
    assert datasheet_gases() == ["CO"]
