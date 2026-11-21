"""Tests for the station list capacity emitted by the wifi codegen."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import wifi
from esphome.core import CORE, EsphomeError
from esphome.coroutine import CoroPriority, coroutine_with_priority


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


@pytest.mark.parametrize("count", [0, -1])
def test_request_below_one_raises(count: int) -> None:
    """A request can only add slots, never take away the YAML networks' slots."""
    with pytest.raises(ValueError, match="at least 1"):
        wifi.request_wifi_network_slots(count)


def test_request_from_a_later_job_is_counted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A component whose code generation runs after wifi's still gets its slots."""

    @coroutine_with_priority(CoroPriority.LATE)
    async def late_request() -> None:
        wifi.request_wifi_network_slots(4)

    CORE.add_job(late_request)
    generate_main(component_config_path("three_networks.yaml"))

    defines = {define.name: define.value for define in CORE.defines}
    assert str(defines["WIFI_NETWORK_COUNT"]) == "7"


def test_request_after_the_count_is_emitted_raises(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Once the station list size is in the build, a new request cannot grow it."""
    generate_main(component_config_path("three_networks.yaml"))

    with pytest.raises(ValueError, match="after the count define was emitted"):
        wifi.request_wifi_network_slots(1)
