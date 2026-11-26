"""Tests for the station list capacity emitted by the wifi codegen."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import wifi
from esphome.core import CORE, EsphomeError
from esphome.coroutine import CoroPriority, coroutine_with_priority


def network_count_defines() -> list[str]:
    """Every WIFI_NETWORK_COUNT value the build emitted."""
    return [str(d.value) for d in CORE.defines if d.name == "WIFI_NETWORK_COUNT"]


@pytest.mark.parametrize(
    ("config_file", "requests", "count"),
    [
        # set_sta() stores one network at run time even when none is configured
        ("ap_only.yaml", [], 1),
        ("bare.yaml", [], 1),
        ("three_networks.yaml", [], 3),
        ("ap_only.yaml", [5], 5),
        ("three_networks.yaml", [5], 8),
        ("three_networks.yaml", [2, 4], 9),
        ("three_networks.yaml", [wifi.MAX_WIFI_NETWORKS - 3], wifi.MAX_WIFI_NETWORKS),
    ],
)
def test_network_count_define(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    requests: list[int],
    count: int,
) -> None:
    """The list holds the configured networks plus every request, at least one."""
    for request in requests:
        wifi.request_wifi_network_slots(request)

    generate_main(component_config_path(config_file))

    assert network_count_defines() == [str(count)]


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

    assert network_count_defines() == ["7"]


@pytest.mark.parametrize("config_file", ["ap_only.yaml", "three_networks.yaml"])
def test_request_after_the_count_is_emitted_raises(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """A request from a job that runs after the size is emitted cannot grow it."""

    @coroutine_with_priority(CoroPriority.FINAL)
    async def final_request() -> None:
        wifi.request_wifi_network_slots(4)

    # Queued from a LATE job, so it runs after wifi's FINAL jobs
    @coroutine_with_priority(CoroPriority.LATE)
    async def schedule() -> None:
        CORE.add_job(final_request)

    CORE.add_job(schedule)

    with pytest.raises(ValueError, match="after the count define was emitted"):
        generate_main(component_config_path(config_file))
