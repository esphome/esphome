"""Tests for the exponential_moving_average sensor."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.exponential_moving_average.sensor import CONFIG_SCHEMA


def test_default_alpha_and_restore(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without alpha or time_constant, alpha defaults to 0.1 and restore is on."""
    main_cpp = generate_main(
        component_config_path("exponential_moving_average_test.yaml")
    )

    assert (
        "new(ema_default) exponential_moving_average::ExponentialMovingAverageSensor(source_sensor);"
        in main_cpp
    )
    assert "ema_default->set_alpha(0.1f);" in main_cpp
    assert "ema_default->set_restore(true);" in main_cpp


def test_alpha(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(
        component_config_path("exponential_moving_average_test.yaml")
    )

    assert "ema_alpha->set_alpha(0.25f);" in main_cpp
    assert "ema_alpha->set_time_constant" not in main_cpp


def test_time_constant_replaces_alpha(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(
        component_config_path("exponential_moving_average_test.yaml")
    )

    assert "ema_time_constant->set_time_constant(300000);" in main_cpp
    assert "ema_time_constant->set_alpha" not in main_cpp
    assert "ema_time_constant->set_restore(false);" in main_cpp
    assert "ema_time_constant->set_time_weighting" not in main_cpp


def test_time_weighting(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(
        component_config_path("exponential_moving_average_test.yaml")
    )

    assert "ema_linear->set_time_constant(30000);" in main_cpp
    assert (
        "ema_linear->set_time_weighting(exponential_moving_average::TIME_WEIGHTING_LINEAR);"
        in main_cpp
    )


def test_properties_inherited_from_source(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Unset properties come from the source sensor, with one extra decimal; set ones are kept."""
    main_cpp = generate_main(
        component_config_path("exponential_moving_average_test.yaml")
    )

    assert "ema_default->set_accuracy_decimals(2);" in main_cpp
    assert "ema_alpha->set_accuracy_decimals(3);" in main_cpp
    default_line = next(
        line for line in main_cpp.splitlines() if '"EMA Default"' in line
    )
    alpha_line = next(line for line in main_cpp.splitlines() if '"EMA Alpha"' in line)
    assert "°C" in default_line
    assert "temperature" in default_line
    assert "K" in alpha_line


def test_reset_action(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(
        component_config_path("exponential_moving_average_test.yaml")
    )

    assert "::ema_default->reset();" in main_cpp


def test_alpha_and_time_constant_are_exclusive() -> None:
    with pytest.raises(cv.Invalid, match="Cannot specify more than one of"):
        CONFIG_SCHEMA(
            {
                "id": "ema",
                "name": "EMA",
                "sensor": "source",
                "alpha": 0.5,
                "time_constant": "1min",
            }
        )


@pytest.mark.parametrize("alpha", [0, -0.1, 1.5])
def test_alpha_out_of_range(alpha: float) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"id": "ema", "name": "EMA", "sensor": "source", "alpha": alpha})


@pytest.mark.parametrize("alpha", [0.01, 1])
def test_alpha_in_range(alpha: float) -> None:
    config = CONFIG_SCHEMA(
        {"id": "ema", "name": "EMA", "sensor": "source", "alpha": alpha}
    )

    assert config["alpha"] == alpha


def test_time_weighting_requires_time_constant() -> None:
    with pytest.raises(cv.Invalid, match="can only be used with 'time_constant'"):
        CONFIG_SCHEMA(
            {
                "id": "ema",
                "name": "EMA",
                "sensor": "source",
                "time_weighting": "previous",
            }
        )


@pytest.mark.parametrize("weighting", ["new", "previous", "linear", "LINEAR"])
def test_time_weighting_values(weighting: str) -> None:
    config = CONFIG_SCHEMA(
        {
            "id": "ema",
            "name": "EMA",
            "sensor": "source",
            "time_constant": "1min",
            "time_weighting": weighting,
        }
    )

    assert config["time_weighting"] == weighting.lower()


def test_time_weighting_rejects_unknown_value() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(
            {
                "id": "ema",
                "name": "EMA",
                "sensor": "source",
                "time_constant": "1min",
                "time_weighting": "trapezoid",
            }
        )
