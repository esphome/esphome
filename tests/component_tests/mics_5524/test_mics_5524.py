"""Tests for the MiCS-5524 gas sensor code generation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.mics_5524.coefficients import GASES, VendorCurve
from esphome.components.mics_5524.sensor import CONFIG_SCHEMA
from esphome.const import (
    CONF_AMMONIA,
    CONF_CARBON_MONOXIDE,
    CONF_ETHANOL,
    CONF_HYDROGEN,
    CONF_METHANE,
)


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
    """The curve is passed to the constructor and the sub-sensor to its own hub."""
    main_cpp = generate_main(component_config_path("mics_5524.yaml"))

    # The vendor label, then threshold, gain, minimum and maximum ppm of the gas.
    assert (
        'new(mics_5524_co) mics_5524::MiCS5524GasSensor("CO", 0.425f, '
        "0.000405f, 1.0f, 500.0f);" in main_cpp
    )
    assert "mics_5524_boot->register_gas_sensor(mics_5524_co);" in main_cpp
    # A gas without `max_ppm:` keeps the measuring range of the vendor.
    assert (
        'new(mics_5524_ch4) mics_5524::MiCS5524GasSensor("CH4", 0.786f, '
        "2.3e-05f, 1000.0f, 25000.0f);" in main_cpp
    )
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

    assert "mics_5524_boot->set_vcc(5.0f);" in main_cpp
    assert "mics_5524_boot->set_voltage_multiplier(1.5f);" in main_cpp
    assert "mics_5524_boot->set_samples(3);" in main_cpp
    assert "mics_5524_boot->set_warmup_time(180000);" in main_cpp
    assert "mics_5524_boot->set_calibration(5000, 3, 20);" in main_cpp
    assert "mics_5524_fixed->set_air_reference(3.6f);" in main_cpp
    # `air_reference:` and `calibration:` exclude each other, so the sensor with the
    # hard-coded reference is never calibrated at boot.
    assert "mics_5524_fixed->set_calibration(" not in main_cpp


def test_air_reference_and_calibration_exclude_each_other() -> None:
    """Two references for one sensor would silently pick one, so it is an error."""
    with pytest.raises(cv.Invalid, match="Cannot specify more than one of"):
        CONFIG_SCHEMA(
            {
                "id": "mics_5524",
                "voltage": "mics_5524_adc",
                "hydrogen": {"name": "MiCS-5524 H2 (reference)"},
                "air_reference": 3.6,
                "calibration": {"delay": "5s"},
            }
        )


@pytest.mark.parametrize("air_reference", [5.0, 6.0])
def test_air_reference_at_or_above_vcc_is_rejected(air_reference: float) -> None:
    """A reference at or above the supply would report every gas as 0 ppm forever."""
    with pytest.raises(cv.Invalid, match="must be below vcc"):
        CONFIG_SCHEMA(
            {
                "id": "mics_5524",
                "voltage": "mics_5524_adc",
                "vcc": 5.0,
                "hydrogen": {"name": "MiCS-5524 H2 (reference above vcc)"},
                "air_reference": air_reference,
            }
        )


def test_air_reference_below_vcc_is_accepted() -> None:
    config = CONFIG_SCHEMA(
        {
            "id": "mics_5524",
            "voltage": "mics_5524_adc",
            "vcc": 5.0,
            "hydrogen": {"name": "MiCS-5524 H2 (reference)"},
            "air_reference": 3.6,
        }
    )

    assert config["air_reference"] == 3.6


def test_at_least_one_gas_is_required() -> None:
    """A sensor without a gas has nothing to publish."""
    with pytest.raises(cv.Invalid, match="Must contain at least one of"):
        CONFIG_SCHEMA({"id": "mics_5524", "voltage": "mics_5524_adc", "vcc": 5.0})


def test_max_ppm_above_the_vendor_range_is_rejected() -> None:
    """A ceiling the conversion can never reach is a configuration error."""
    with pytest.raises(cv.Invalid, match="above the CO measuring range"):
        CONFIG_SCHEMA(
            {
                "id": "mics_5524",
                "voltage": "mics_5524_adc",
                "carbon_monoxide": {
                    "name": "MiCS-5524 CO (rejected)",
                    "max_ppm": 2000.0,
                },
                "air_reference": 3.6,
            }
        )


def test_max_ppm_at_the_vendor_range_is_accepted() -> None:
    config = CONFIG_SCHEMA(
        {
            "id": "mics_5524",
            "voltage": "mics_5524_adc",
            "carbon_monoxide": {"name": "MiCS-5524 CO (accepted)", "max_ppm": 1000.0},
            "air_reference": 3.6,
        }
    )

    assert config["carbon_monoxide"]["max_ppm"] == 1000.0


def test_max_ppm_defaults_to_the_vendor_range_of_the_gas() -> None:
    config = CONFIG_SCHEMA(
        {
            "id": "mics_5524",
            "voltage": "mics_5524_adc",
            "carbon_monoxide": {"name": "MiCS-5524 CO (default)"},
            "air_reference": 3.6,
        }
    )

    assert config["carbon_monoxide"]["max_ppm"] == 1000.0


def test_calibration_cannot_sample_faster_than_the_loop() -> None:
    """The main loop runs every ~16 ms, so a shorter spacing samples nothing extra."""
    with pytest.raises(cv.Invalid, match="value must be at least 16ms"):
        CONFIG_SCHEMA(
            {
                "id": "mics_5524",
                "voltage": "mics_5524_adc",
                "hydrogen": {"name": "MiCS-5524 H2 (tick)"},
                "calibration": {"sample_interval": "10ms"},
            }
        )


def test_voltage_multiplier_defaults_to_one() -> None:
    config = CONFIG_SCHEMA(
        {
            "id": "mics_5524",
            "voltage": "mics_5524_adc",
            "hydrogen": {"name": "MiCS-5524 H2 (default)"},
            "air_reference": 3.6,
        }
    )

    assert config["voltage_multiplier"] == 1.0


def test_vendor_table_is_the_datasheet_one() -> None:
    """The whole table is pinned, not only the gases a config file happens to use."""
    expected = {
        CONF_CARBON_MONOXIDE: VendorCurve("CO", 0.425, 0.000405, 1.0, 1000.0),
        CONF_HYDROGEN: VendorCurve("H2", 0.279, 0.00026, 1.0, 1000.0),
        CONF_ETHANOL: VendorCurve("C2H5OH", 0.306, 0.00057, 10.0, 500.0),
        CONF_AMMONIA: VendorCurve("NH3", 0.8, 0.0015, 10.0, 500.0),
        CONF_METHANE: VendorCurve("CH4", 0.786, 0.000023, 1000.0, 25000.0),
    }

    assert expected == GASES
