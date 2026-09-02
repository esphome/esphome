"""Tests for the bme68x_bsec2 prefetch extraction."""

from __future__ import annotations

from pathlib import Path

from esphome.components import bme68x_bsec2 as bsec
from esphome.loader import get_component


def test_prefetch_applies_defaults(setup_core: Path) -> None:
    [files] = list(bsec.PREFETCH_FILES([{"model": "bme680"}]))
    assert len(files) == 1
    assert "bme680_iaq_33v_3s_28d" in files[0].url
    assert files[0].path == bsec._compute_local_file_path(files[0].url)


def test_prefetch_normalizes_enum_case(setup_core: Path) -> None:
    [files] = list(
        bsec.PREFETCH_FILES(
            [
                {
                    "model": "BME688",
                    "sample_rate": "ulp",
                    "supply_voltage": "1.8v",
                    "algorithm_output": "REGRESSION",
                    "operating_age": "4D",
                }
            ]
        )
    )
    assert len(files) == 1
    assert "bme688_reg_18v_300s_4d" in files[0].url


def test_prefetch_skips_unknown_values(setup_core: Path) -> None:
    entries = [
        {"model": "bme999"},
        {"model": "bme680", "sample_rate": "TURBO"},
        {"model": "bme680", "algorithm_output": "psychic"},
        {},
    ]
    assert list(bsec.PREFETCH_FILES(entries)) == [[]]


def test_prefetch_matches_validator_url(setup_core: Path) -> None:
    """The hook's URL equals _compute_url over the validated config shape."""
    validated = {
        "model": "bme688",
        "operating_age": "28d",
        "sample_rate": "LP",
        "supply_voltage": "3.3V",
        "algorithm_output": "classification",
    }
    [files] = list(bsec.PREFETCH_FILES([dict(validated)]))
    assert files[0].url == bsec._compute_url(validated)


def test_hook_is_wired_to_the_user_facing_domain() -> None:
    """The i2c domain (the only user-facing one) exposes the hook."""

    component = get_component("bme68x_bsec2_i2c")
    assert component is not None
    assert component.prefetch_files is bsec.PREFETCH_FILES
