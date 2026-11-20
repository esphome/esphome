"""Tests for the station list capacity emitted by the wifi codegen."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import wifi
from esphome.core import CORE, EsphomeError


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


@pytest.mark.parametrize(
    ("config_file", "requests", "count"),
    [
        ("ap_only.yaml", [5], 5),
        ("three_networks.yaml", [5], 8),
        ("three_networks.yaml", [2, 4], 9),
    ],
)
def test_requested_slots_add_to_network_count(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    requests: list[int],
    count: int,
) -> None:
    """Slots components request add to the configured networks and to each other."""
    for request in requests:
        wifi.request_wifi_network_slots(request)

    generate_main(component_config_path(config_file))

    defines = {define.name: define.value for define in CORE.defines}
    assert str(defines["WIFI_NETWORK_COUNT"]) == str(count)


def test_requested_slots_over_the_limit_raise(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Networks and requested slots together cannot exceed MAX_WIFI_NETWORKS."""
    wifi.request_wifi_network_slots(wifi.MAX_WIFI_NETWORKS - 2)

    with pytest.raises(EsphomeError, match="add up to 128"):
        generate_main(component_config_path("three_networks.yaml"))
