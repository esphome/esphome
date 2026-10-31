"""Tests for the bp5758d output code generation."""

from collections.abc import Callable
from pathlib import Path


def test_channels_use_apply_outputs(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Channels are 0-based on the hub and the current is baked into the call."""
    main_cpp = generate_main(component_config_path("bp5758d.yaml"))

    assert "BP5758D::Channel" not in main_cpp
    assert "new(out_red) output::ApplyFloatOutput<esphome__out_red__fn>(0);" in main_cpp
    assert (
        "new(out_white) output::ApplyFloatOutput<esphome__out_white__fn>(4);"
        in main_cpp
    )
    assert main_cpp.count("::hub->write_channel(index, state, 15);") == 1
    assert main_cpp.count("::hub->write_channel(index, state, 10);") == 1


def test_channels_with_equal_current_share_one_function(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("bp5758d.yaml"))

    assert (
        "new(out_green) output::ApplyFloatOutput<esphome__out_red__fn>(1);" in main_cpp
    )
    assert "esphome__out_green__fn" not in main_cpp
