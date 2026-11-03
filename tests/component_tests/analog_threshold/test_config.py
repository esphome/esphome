"""Validate analog threshold ordering without restricting dynamic thresholds."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.analog_threshold.binary_sensor import CONFIG_SCHEMA
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE, Lambda


@pytest.mark.parametrize(
    ("lower", "upper"),
    [(90, 110), (0, 0), (-10, -5), ("0.27", "0.78")],
)
def test_valid_static_thresholds(lower: float | str, upper: float | str) -> None:
    config = CONFIG_SCHEMA(
        {
            "name": "Threshold",
            "sensor_id": "source_sensor",
            "threshold": {"lower": lower, "upper": upper},
        }
    )
    assert config["threshold"] == {"lower": float(lower), "upper": float(upper)}


@pytest.mark.parametrize(("lower", "upper"), [(0.78, 0.27), (1, 0), (-1, -2)])
def test_reversed_static_thresholds_are_rejected(lower: float, upper: float) -> None:
    with pytest.raises(
        cv.Invalid, match="Lower threshold must be less than or equal"
    ) as err:
        CONFIG_SCHEMA(
            {
                "name": "Threshold",
                "sensor_id": "source_sensor",
                "threshold": {"lower": lower, "upper": upper},
            }
        )
    assert err.value.path == ["threshold", "lower"]


@pytest.mark.parametrize(
    "threshold",
    [
        100,
        Lambda("return 100;"),
        {"lower": Lambda("return 90;"), "upper": 110},
        {"lower": 90, "upper": Lambda("return 110;")},
        {"lower": Lambda("return 90;"), "upper": Lambda("return 110;")},
    ],
)
def test_shorthand_and_dynamic_thresholds_are_accepted(
    threshold: float | Lambda | dict[str, float | Lambda],
) -> None:
    config = CONFIG_SCHEMA(
        {
            "name": "Threshold",
            "sensor_id": "source_sensor",
            "threshold": threshold,
        }
    )
    if isinstance(threshold, dict):
        for key, value in threshold.items():
            if isinstance(value, Lambda):
                assert isinstance(config["threshold"][key], Lambda)
            else:
                assert config["threshold"][key] == value
    elif isinstance(threshold, Lambda):
        assert isinstance(config["threshold"], Lambda)
    else:
        assert config["threshold"] == float(threshold)


def test_reversed_thresholds_report_the_yaml_option(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("reversed.yaml")
    assert read_config({}) is None
    assert "Lower threshold must be less than or equal" in capsys.readouterr().out
