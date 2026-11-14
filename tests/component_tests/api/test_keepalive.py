"""Tests for the api keepalive option codegen."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


def _defines() -> dict[str, str]:
    return {define.name: str(define.value) for define in CORE.defines}


@pytest.mark.parametrize(
    "config_file", ["bare.yaml", "defaults.yaml", "keepalive_subsecond.yaml"]
)
def test_default_keepalive_emits_no_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """The default (also after truncation to whole seconds) keeps the field out of the build."""
    generate_main(component_config_path(config_file))

    defines = _defines()
    assert "USE_API_KEEPALIVE_INTERVAL" not in defines
    assert "API_KEEPALIVE_TIMEOUT_MS" not in defines


def test_custom_keepalive_emits_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A non default keepalive enables the HelloResponse field and sets the interval."""
    generate_main(component_config_path("keepalive.yaml"))

    defines = _defines()
    assert "USE_API_KEEPALIVE_INTERVAL" in defines
    assert defines["API_KEEPALIVE_TIMEOUT_MS"] == "300000"


def test_keepalive_is_truncated_to_whole_seconds(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The device uses the same whole-second interval that it advertises."""
    generate_main(component_config_path("keepalive_truncated.yaml"))

    assert _defines()["API_KEEPALIVE_TIMEOUT_MS"] == "90000"
