"""Tests for the output platform codegen."""

from collections.abc import Callable
from pathlib import Path
from types import ModuleType

import pytest

from esphome import config_validation as cv
from esphome.components.mcp4728 import output as mcp4728_output
from esphome.components.pca9685 import output as pca9685_output
from esphome.components.sm2135 import output as sm2135_output
from esphome.config import Config, read_config
from esphome.const import CONF_CHANNEL, CONF_PLATFORM, PlatformFramework
from esphome.core import CORE, ID

from ..types import SetCoreConfigCallable


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


def test_default_min_power_keeps_scaling_fields(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """ac_dimmer reads min_power_ directly, so the define must stay on for min_power 0%."""
    main_cpp = generate_main(component_config_path("ac_dimmer_min_power_zero.yaml"))

    assert "dimmer->set_min_power(" not in main_cpp
    assert "USE_OUTPUT_FLOAT_POWER_SCALING" in {d.name for d in CORE.defines}


def test_duplicate_hub_channel_is_rejected(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """Two outputs on one hub channel would overwrite each other's value."""
    CORE.config_path = component_config_path("pca9685_duplicate_channel.yaml")
    assert read_config({}) is None
    assert "Channel '0' of 'pwm_a' is already used by another output" in (
        capsys.readouterr().out
    )


def _entries(platform: str, hub_key: str, *hub_channels: tuple[str, object]) -> list:
    return [
        {CONF_PLATFORM: platform, hub_key: ID(hub), CONF_CHANNEL: channel}
        for hub, channel in hub_channels
    ]


@pytest.mark.parametrize(
    ("module", "platform", "hub_key", "channel", "other"),
    [
        (pca9685_output, "pca9685", "pca9685_id", 3, 4),
        (mcp4728_output, "mcp4728", "mcp4728_id", "A", "B"),
        (sm2135_output, "sm2135", "sm2135_id", 1, 2),
    ],
)
def test_hub_channel_must_be_unique(
    set_core_config: SetCoreConfigCallable,
    module: ModuleType,
    platform: str,
    hub_key: str,
    channel: object,
    other: object,
) -> None:
    """Each channel of a hub takes one output; another hub or channel is fine."""
    entries = _entries(
        platform, hub_key, ("a", channel), ("a", other), ("b", channel), ("a", channel)
    )
    full = Config()
    full["output"] = entries
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)
    for entry in entries[1:3]:
        module.FINAL_VALIDATE_SCHEMA(entry)
    with pytest.raises(cv.Invalid, match=f"Channel '{channel}' of 'a' is already used"):
        module.FINAL_VALIDATE_SCHEMA(entries[3])
