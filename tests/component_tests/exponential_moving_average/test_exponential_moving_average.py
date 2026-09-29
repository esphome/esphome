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


def test_properties_inherited_from_source(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Unset properties come from the source sensor; set ones are kept."""
    main_cpp = generate_main(
        component_config_path("exponential_moving_average_test.yaml")
    )

    assert "ema_default->set_accuracy_decimals(1);" in main_cpp
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
