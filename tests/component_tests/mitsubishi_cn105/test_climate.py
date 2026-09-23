"""Tests for Mitsubishi CN105 climate configuration migration diagnostics."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.mitsubishi_cn105 import climate
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.yaml_util import load_yaml


def test_top_level_hub_rejects_leftover_legacy_climate_keys(
    component_fixture_path: Callable[[str], Path],
) -> None:
    config = load_yaml(
        component_fixture_path("top_level_hub_with_legacy_climate_keys.yaml")
    )
    CORE.raw_config = config

    with pytest.raises(cv.Invalid) as exc_info:
        climate.CONFIG_SCHEMA(config["climate"][0])

    message = str(exc_info.value)
    assert "'current_temperature_min_interval'" in message
    assert "'uart_id'" in message
    assert "'update_interval'" in message
    assert "top-level 'mitsubishi_cn105:' block" in message
    assert "'telemetry_request_min_interval'" in message
