"""Exercise XGZP68xx variant selection, validation and code generation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE


def test_legacy_variant_is_default(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("legacy.yaml"))
    assert ") xgzp68xx::XGZP68XXComponent()" in source
    assert "set_i2c_address(0x6D)" in source
    assert "set_pressure_oversampling(xgzp68xx::XGZP68XX_OVERSAMPLING_1024X)" in source
    assert "set_k_value(4096)" in source
    assert "XGZP6899DC" not in source


def test_xgzp6899dc_variant(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("xgzp6899dc.yaml"))
    assert ") xgzp68xx::XGZP6899DCComponent(1000.0f)" in source
    assert "set_i2c_address(0x58)" in source
    # 8192X is CFG_OSR code 0b101
    assert "set_pressure_oversampling(5)" in source
    assert "set_temperature_sensor" in source
    assert "set_k_value" not in source


def test_xgzp6899dc_keeps_factory_oversampling(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("xgzp6899dc_no_oversampling.yaml"))
    assert ") xgzp68xx::XGZP6899DCComponent(1000.0f)" in source
    assert "set_pressure_oversampling" not in source


@pytest.mark.parametrize(
    ("config_file", "message"),
    [
        ("xgzp6899dc_k_value.yaml", "[k_value] is an invalid option"),
        ("xgzp6899dc_missing_range.yaml", "required key not provided"),
        (
            "xgzp6899dc_inverted_range.yaml",
            "'pressure_max' must be greater than 'pressure_min'",
        ),
        (
            "legacy_pressure_range.yaml",
            "'pressure_min' is only used with 'variant: xgzp6899dc'",
        ),
    ],
)
def test_invalid_configs_are_rejected(
    config_file: str,
    message: str,
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path(config_file)
    assert read_config({}) is None
    assert message in capsys.readouterr().out


def test_xgzp6899dc_temperature_only(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("xgzp6899dc_temperature_only.yaml"))
    assert ") xgzp68xx::XGZP6899DCComponent(10000.0f)" in source
    assert "set_temperature_sensor" in source
    assert "set_pressure_sensor" not in source
    assert "set_pressure_oversampling" not in source
