"""The rh_correction filter: schema and the constructor call it generates."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.sensor import validate_filters
import esphome.config_validation as cv


@pytest.mark.parametrize(
    "filters",
    [
        pytest.param([{"rh_correction": {"sensor_id": "t"}}], id="no_offset"),
        pytest.param([{"rh_correction": {"temperature_offset": -7.3}}], id="no_sensor"),
        pytest.param(
            [
                {
                    "rh_correction": {
                        "sensor_id": "t",
                        "temperature_offset": -7.3,
                        "use_fahrenheit": "maybe",
                    }
                }
            ],
            id="bad_fahrenheit",
        ),
    ],
)
def test_rh_correction_rejects_incomplete_config(filters: list[dict]) -> None:
    with pytest.raises(cv.Invalid):
        validate_filters(filters)


def test_rh_correction_defaults_to_celsius() -> None:
    config = validate_filters(
        [{"rh_correction": {"sensor_id": "t", "temperature_offset": -7.3}}]
    )
    assert config[0]["rh_correction"]["use_fahrenheit"] is False


def test_rh_correction_codegen(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main("tests/component_tests/sensor/rh_correction.yaml")

    # A constant offset is wrapped in a stateless lambda so the filter stores a function pointer
    assert re.search(
        r"sensor::RHCorrectionFilter\(\[\]\(\) -> float \{\s*return -7\.3f;\s*\}, "
        r"temperature, false\);\s*constant_offset->add_filter\(",
        main_cpp,
    )
    # A templated offset keeps the user's lambda body and passes use_fahrenheit through
    assert re.search(
        r"sensor::RHCorrectionFilter\(\[\]\(\) -> float \{[^}]*return -7\.3;\s*\}, "
        r"temperature, true\);\s*templated_offset->add_filter\(",
        main_cpp,
    )
