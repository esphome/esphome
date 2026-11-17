"""Tests for the ld2410 background correction options."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import load_config
from esphome.core import CORE


@pytest.mark.parametrize(
    ("yaml_file", "key"),
    [
        ("with_button.yaml", None),
        ("calibration_without_button.yaml", "calibration"),
        ("calibration_result_without_button.yaml", "calibration_result"),
        ("duration_without_button.yaml", "background_correction_duration"),
    ],
)
def test_background_correction_options_require_the_button(
    component_config_path: Callable[[str], Path], yaml_file: str, key: str | None
) -> None:
    CORE.config_path = component_config_path(yaml_file)
    errors = load_config({}).errors
    if key is None:
        assert not errors
        return
    assert len(errors) == 1
    assert "requires the ld2410 'background_correction' button" in str(errors[0])
    assert errors[0].path[-1] == key
