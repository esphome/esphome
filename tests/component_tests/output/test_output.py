"""Tests for the output platform codegen."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_default_power_limits_are_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """max_power 100% and min_power 0% already live in the C++ initializers."""
    main_cpp = generate_main(component_config_path("power_limits.yaml"))

    assert "default_power->set_max_power(" not in main_cpp
    assert "default_power->set_min_power(" not in main_cpp
    assert "custom_power->set_max_power(0.9f);" in main_cpp
    assert "custom_power->set_min_power(0.01f);" in main_cpp
    assert "USE_OUTPUT_FLOAT_POWER_SCALING" in {d.name for d in CORE.defines}
