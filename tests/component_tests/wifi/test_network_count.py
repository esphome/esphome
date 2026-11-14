"""Tests for the station list capacity emitted by the wifi codegen."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize(
    ("config_file", "count"),
    [
        # set_sta() stores one network at run time even when none is configured
        ("ap_only.yaml", 1),
        ("bare.yaml", 1),
        ("three_networks.yaml", 3),
    ],
)
def test_network_count_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    count: int,
) -> None:
    """The station list holds every configured network and at least one."""
    generate_main(component_config_path(config_file))
    defines = {define.name: define.value for define in CORE.defines}
    assert str(defines["WIFI_NETWORK_COUNT"]) == str(count)
