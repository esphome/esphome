"""Tests for the MiCS-5524 gas sensor code generation."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome import config_validation as cv
from esphome.components.mics_5524_gas_sensor.coefficients import (
    GASES,
    DatasheetCurve,
    GasDefinition,
    VendorCurve,
    datasheet_gases,
)
from esphome.components.mics_5524_gas_sensor.sensor import (
    CONFIG_SCHEMA,
    _voltage_multiplier,
)
from esphome.const import (
    CONF_AMMONIA,
    CONF_CARBON_MONOXIDE,
    CONF_ETHANOL,
    CONF_HYDROGEN,
    CONF_METHANE,
)

GENERATED_ADC = "_generated_adc"


def _gas_arguments(main_cpp: str, variable: str) -> list[str]:
    """Arguments of the constructor call that creates one gas sub-sensor."""
    match = re.search(
        rf"new\({variable}\) mics_5524_gas_sensor::MiCS5524GasSensor\((.*?)\);",
        main_cpp,
    )
    assert match is not None, f"{variable} was not constructed"
    return [argument.strip() for argument in match.group(1).split(",")]


def _gas_constants(arguments: list[str]) -> list[float]:
    """The numeric arguments of a constructor call, read as floats.

    Comparing numbers rather than their text keeps the test about the values and not
    about how code generation happens to format them.
    """
    assert arguments[0].startswith('"')
    return [float(argument.removesuffix("f")) for argument in arguments[1:]]


def test_voltage_source_is_a_constructor_argument(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Every reading needs the sampler, so it is passed at construction."""
    main_cpp = generate_main(component_config_path("mics_5524_gas_sensor.yaml"))

    assert (
        "new(mics_5524_boot) mics_5524_gas_sensor::MiCS5524Component(mics_5524_adc);"
        in main_cpp
    )
    assert (
        "new(mics_5524_fixed) mics_5524_gas_sensor::MiCS5524Component(mics_5524_adc);"
        in main_cpp
    )
    assert "set_source(" not in main_cpp


def test_every_gas_becomes_a_sub_sensor_of_its_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The constants are passed to the constructor and the sub-sensor to its own hub."""
    main_cpp = generate_main(component_config_path("mics_5524_gas_sensor.yaml"))

    # The gas label, the conversion model (0 = dfrobot), the vendor threshold and gain,
    # the datasheet coefficients (unused here) and the measuring range.
    co = _gas_arguments(main_cpp, "mics_5524_co")
    assert co[0] == '"CO"'
    assert _gas_constants(co) == [0, 0.425, 0.000405, 0.0, 0.0, 1.0, 500.0]
    assert "mics_5524_boot->register_gas_sensor(mics_5524_co);" in main_cpp

    # A gas without `max_ppm:` keeps the measuring range of the vendor.
    ch4 = _gas_arguments(main_cpp, "mics_5524_ch4")
    assert ch4[0] == '"CH4"'
    assert _gas_constants(ch4) == [0, 0.786, 0.000023, 0.0, 0.0, 1000.0, 25000.0]
    assert "mics_5524_boot->register_gas_sensor(mics_5524_ch4);" in main_cpp

    # The gas belongs to the sensor it is configured under, not to the first one.
    assert "mics_5524_fixed->register_gas_sensor(mics_5524_h2);" in main_cpp
    assert "mics_5524_boot->register_gas_sensor(mics_5524_h2);" not in main_cpp


def test_shared_options_reach_the_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The analog front end is configured once per physical sensor."""
    main_cpp = generate_main(component_config_path("mics_5524_gas_sensor.yaml"))

    assert "mics_5524_boot->set_conversion(0);" in main_cpp
    assert "mics_5524_boot->set_vcc(5.0f);" in main_cpp
    assert "mics_5524_boot->set_rl(10.0f);" in main_cpp
    assert "mics_5524_boot->set_voltage_multiplier(1.5f);" in main_cpp
    assert "mics_5524_boot->set_samples(3);" in main_cpp
    assert "mics_5524_boot->set_warmup_time(180000);" in main_cpp
    assert "mics_5524_boot->set_calibration(5000, 3, 20);" in main_cpp
    assert "mics_5524_fixed->set_air_reference(3.6f);" in main_cpp
    # `air_reference:` and `calibration:` exclude each other, so the sensor with the
    # hard-coded reference is never calibrated at boot.
    assert "mics_5524_fixed->set_calibration(" not in main_cpp


def test_datasheet_conversion_uses_the_shipped_coefficients(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """One conversion model per sensor, and the coefficients of CO are the shipped ones."""
    main_cpp = generate_main(component_config_path("mics_5524_gas_sensor.yaml"))

    # 1 = datasheet, a = 6.3, b = -1.1, range 0 .. 2000 ppm.
    co = _gas_arguments(main_cpp, "mics_5524_co_ds")
    assert co[0] == '"CO"'
    assert _gas_constants(co) == [1, 0.0, 0.0, 6.3, -1.1, 0.0, 2000.0]
    assert "mics_5524_pin->register_gas_sensor(mics_5524_co_ds);" in main_cpp
    assert "mics_5524_pin->set_conversion(1);" in main_cpp
    assert "mics_5524_pin->set_r0(10.0f);" in main_cpp
    # The datasheet model divides by the load resistor, the vendor model does not know it.
    assert "mics_5524_pin->set_rl(10.0f);" in main_cpp


def test_diagnostics_reach_the_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The ratio, RS and voltage of every reading can be published to their own sensor."""
    main_cpp = generate_main(component_config_path("mics_5524_gas_sensor.yaml"))

    assert "mics_5524_pin->set_ratio_sensor(mics_5524_ratio);" in main_cpp
    assert "mics_5524_pin->set_rs_sensor(mics_5524_rs);" in main_cpp
    assert "mics_5524_pin->set_voltage_sensor(mics_5524_volts);" in main_cpp


def test_pin_creates_an_internal_adc(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A `pin:` configuration owns its ADC, and the divider is part of the reading."""
    main_cpp = generate_main(component_config_path("mics_5524_gas_sensor.yaml"))

    assert "new(mics_5524_pin_adc) adc::ADCSensor();" in main_cpp
    assert (
        "new(mics_5524_pin) mics_5524_gas_sensor::MiCS5524Component(mics_5524_pin_adc);"
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


def test_max_ppm_defaults_to_the_vendor_range_of_the_gas() -> None:
    config = CONFIG_SCHEMA(_hub(carbon_monoxide={"name": "MiCS-5524 CO (default)"}))

    assert config["carbon_monoxide"]["max_ppm"] == 1000.0


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

    assert config["voltage_multiplier"] == 1.0
    assert _voltage_multiplier(config) == 1.0


def test_voltage_divider_derives_the_multiplier() -> None:
    """The divider the user wired is turned into the multiplier of every reading."""
    config = CONFIG_SCHEMA(
        _hub(voltage_divider={"r1": 10000.0, "r2": 20000.0}, adc_input_max=3.34)
    )

    assert _voltage_multiplier(config) == 1.5


def test_voltage_divider_rejects_an_explicit_multiplier() -> None:
    """The divider already says what the multiplier is, so the two would contradict."""
    with pytest.raises(cv.Invalid, match="not both"):
        CONFIG_SCHEMA(
            _hub(
                voltage_divider={"r1": 10000.0, "r2": 20000.0},
                voltage_multiplier=1.5,
                adc_input_max=3.34,
            )
        )


def test_voltage_divider_above_the_adc_limit_is_rejected() -> None:
    """A 10k/20k divider on a 5 V sensor puts 3.33 V on a classic ESP32 ADC."""
    with pytest.raises(cv.Invalid, match="above the 3.3 V measuring range"):
        CONFIG_SCHEMA(_hub(voltage_divider={"r1": 10000.0, "r2": 20000.0}))


def test_voltage_divider_below_the_declared_limit_is_accepted() -> None:
    """A sampler with a wider range declares it, and the same divider then passes."""
    config = CONFIG_SCHEMA(
        _hub(
            voltage_divider={"r1": 10000.0, "r2": 20000.0},
            adc_input_max=3.34,
            adc_pin_max=3.4,
        )
    )

    assert config["voltage_divider"]["r1"] == 10000.0


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
    with pytest.raises(cv.Invalid, match="remove them or switch the conversion"):
        CONFIG_SCHEMA(
            _hub(hydrogen={"name": "MiCS-5524 H2 (stray)", "coefficient_a": 12.0})
        )


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
            datasheet=DatasheetCurve(6.3, -1.1, 1000.0),
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
