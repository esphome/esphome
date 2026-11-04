"""Keep strobe color-temperature intent through validation and code generation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.light.effects import STROBE_COLOR_SCHEMA


@pytest.mark.parametrize("temperature", ["4100K", "4000K"])
def test_temperature_steps_do_not_add_explicit_white_channels(temperature: str) -> None:
    config = STROBE_COLOR_SCHEMA(
        {"color_temperature": temperature, "duration": "500ms"}
    )
    assert "cold_white" not in config
    assert "warm_white" not in config


@pytest.mark.parametrize(
    "channels",
    [
        {"cold_white": "25%", "warm_white": "50%"},
        {"cold_white": "0%"},
        {"warm_white": "100%"},
    ],
)
def test_explicit_white_channels_are_preserved(channels: dict[str, str]) -> None:
    config = STROBE_COLOR_SCHEMA(
        {"color_temperature": "4100K", "duration": "500ms", **channels}
    )
    for channel, percentage in channels.items():
        assert config[channel] == float(percentage.removesuffix("%")) / 100


def test_codegen_distinguishes_temperature_from_explicit_channels(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("strobe_temperature.yaml"))
    assert source.count(".use_color_temperature = true,") == 2
    assert source.count(".use_color_temperature = false,") == 4
    assert ".cold_white = 0.25f," in source
    assert ".warm_white = 0.5f," in source
    assert ".cold_white = 1.0f," in source
    assert ".warm_white = 1.0f," in source
