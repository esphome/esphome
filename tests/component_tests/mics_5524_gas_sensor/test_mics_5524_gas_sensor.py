"""Tests for the MiCS-5524 gas sensor code generation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.mics_5524_gas_sensor.sensor import CONFIG_SCHEMA


def test_voltage_source_is_a_constructor_argument(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Every reading needs the sampler, so it is passed at construction."""
    main_cpp = generate_main(component_config_path("mics_5524_gas_sensor.yaml"))

    assert (
        "new(mics_5524_co) mics_5524_gas_sensor::MiCS5524GasSensor(mics_5524_adc);"
        in main_cpp
    )
    assert (
        "new(mics_5524_h2) mics_5524_gas_sensor::MiCS5524GasSensor(mics_5524_adc);"
        in main_cpp
    )
    assert "set_source(" not in main_cpp


def test_configured_options_reach_the_component(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The gas table and the YAML overrides are passed to the C++ setters."""
    main_cpp = generate_main(component_config_path("mics_5524_gas_sensor.yaml"))

    assert 'mics_5524_co->set_gas("CO");' in main_cpp
    assert "mics_5524_co->set_threshold(0.425f);" in main_cpp
    assert "mics_5524_co->set_vendor_max_ppm(1000.0f);" in main_cpp
    assert "mics_5524_co->set_min_ppm(1.0f);" in main_cpp
    assert "mics_5524_co->set_max_ppm(500.0f);" in main_cpp
    assert "mics_5524_co->set_voltage_multiplier(1.5f);" in main_cpp
    # `calibration:` is only forwarded when `air_reference:` is not set.
    assert "mics_5524_co->set_calibration(true, 5000, 3);" in main_cpp
    assert "mics_5524_h2->set_air_reference(3.6f);" in main_cpp
    # Only the second sensor has a reference, so nothing is calibrated for it.
    assert "mics_5524_h2->set_calibration(" not in main_cpp


def test_max_ppm_above_the_vendor_range_is_rejected() -> None:
    """A range the conversion can never reach is a configuration error."""
    with pytest.raises(cv.Invalid, match="above the CO measuring range"):
        CONFIG_SCHEMA(
            {
                "id": "mics_5524_co",
                "name": "MiCS-5524 CO",
                "gas": "CO",
                "voltage": "mics_5524_adc",
                "max_ppm": 2000.0,
            }
        )


def test_max_ppm_at_the_vendor_range_is_accepted() -> None:
    config = CONFIG_SCHEMA(
        {
            "id": "mics_5524_co",
            "name": "MiCS-5524 CO",
            "gas": "CO",
            "voltage": "mics_5524_adc",
            "max_ppm": 1000.0,
        }
    )

    assert config["max_ppm"] == 1000.0


def test_max_ppm_must_stay_above_min_ppm() -> None:
    with pytest.raises(cv.Invalid, match="must be greater than min_ppm"):
        CONFIG_SCHEMA(
            {
                "id": "mics_5524_co",
                "name": "MiCS-5524 CO",
                "gas": "CO",
                "voltage": "mics_5524_adc",
                "min_ppm": 600.0,
                "max_ppm": 500.0,
            }
        )


def test_voltage_multiplier_defaults_to_one() -> None:
    config = CONFIG_SCHEMA(
        {
            "id": "mics_5524_h2",
            "name": "MiCS-5524 H2",
            "gas": "H2",
            "voltage": "mics_5524_adc",
        }
    )

    assert config["voltage_multiplier"] == 1.0
