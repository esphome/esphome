"""Tests for the bp5758d output code generation."""

from collections.abc import Callable
from pathlib import Path


def test_channels_use_apply_outputs(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Channels are Apply outputs on 0-based hub channels, not per-channel classes."""
    main_cpp = generate_main(component_config_path("bp5758d.yaml"))

    assert "BP5758D::Channel" not in main_cpp
    assert "new(out_red) output::ApplyFloatOutput<esphome__out_red__fn>(0);" in main_cpp
    assert (
        "new(out_green) output::ApplyFloatOutput<esphome__out_red__fn>(1);" in main_cpp
    )
    assert (
        "new(out_white) output::ApplyFloatOutput<esphome__out_red__fn>(4);" in main_cpp
    )


def test_channels_share_one_function_and_set_their_current(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """One generated function serves every channel; each current is set on the hub."""
    main_cpp = generate_main(component_config_path("bp5758d.yaml"))

    assert main_cpp.count("::hub->write_channel(index, state);") == 1
    assert "esphome__out_green__fn" not in main_cpp
    assert "esphome__out_white__fn" not in main_cpp
    assert "hub->set_channel_current(0, 15);" in main_cpp
    assert "hub->set_channel_current(1, 15);" in main_cpp
    # out_white has no current in the config, so it gets the default
    assert "hub->set_channel_current(4, 10);" in main_cpp
