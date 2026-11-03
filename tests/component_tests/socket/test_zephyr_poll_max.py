"""Tests for the Zephyr poll table size the socket component writes to prj.conf."""

from collections.abc import Callable
from pathlib import Path

from esphome.components.socket import SocketCounts, zephyr_poll_max
from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_PRJ_CONF, KEY_ZEPHYR
from esphome.core import CORE

POLL_MAX = "CONFIG_NET_SOCKETS_POLL_MAX"


def _counts(tcp: int, udp: int, tcp_listen: int) -> SocketCounts:
    return SocketCounts(tcp, udp, tcp_listen, "", "", "")


def test_small_budget_uses_the_minimums() -> None:
    assert zephyr_poll_max(_counts(0, 1, 0)) == 16


def test_large_budget_is_summed() -> None:
    assert zephyr_poll_max(_counts(10, 7, 3)) == 20


def test_nrf52_build_sets_the_poll_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("nrf52_udp.yaml"))
    assert zephyr_data()[KEY_PRJ_CONF][""][POLL_MAX][0] == 16


def test_other_platforms_leave_it_unset(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("esp32_udp.yaml"))
    assert KEY_ZEPHYR not in CORE.data
