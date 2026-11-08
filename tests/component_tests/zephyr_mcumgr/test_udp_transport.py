"""Tests for the prj.conf options the zephyr_mcumgr UDP transport writes."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_PRJ_CONF
from esphome.config import load_config
from esphome.core import CORE

UDP_KEYS = (
    "CONFIG_MCUMGR_TRANSPORT_UDP",
    "CONFIG_MCUMGR_TRANSPORT_UDP_IPV4",
    "CONFIG_MCUMGR_TRANSPORT_UDP_MTU",
    "CONFIG_MCUMGR_TRANSPORT_NETBUF_SIZE",
)


def _udp_values() -> tuple[object, ...]:
    prj_conf = zephyr_data()[KEY_PRJ_CONF][""]
    return tuple(prj_conf.get(key, (None,))[0] for key in UDP_KEYS)


@pytest.mark.parametrize(
    ("fixture", "expected"),
    [
        # Thread's 1280 byte link MTU less the IPv6 (48) or IPv4 (28) and UDP headers
        ("udp_ipv6.yaml", (True, False, 1232, 1232)),
        ("udp_ipv4.yaml", (True, True, 1252, 1252)),
        ("ble_only.yaml", (None, None, None, None)),
        ("uart_only.yaml", (None, None, None, None)),
    ],
)
def test_udp_prj_conf(
    fixture: str,
    expected: tuple[object, ...],
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path(fixture))
    assert _udp_values() == expected
    # BLE and UART need no network; UDP does
    assert ("network" in CORE.config) is (expected[0] is True)


@pytest.mark.parametrize(
    ("fixture", "message"),
    [
        (
            "udp_ipv6_disabled.yaml",
            "zephyr_mcumgr, but 'network: enable_ipv6: false' explicitly disables it",
        ),
        ("udp_no_network.yaml", "'udp' OTA transport requires a network"),
    ],
)
def test_udp_rejected_without_ipv6(
    fixture: str, message: str, component_config_path: Callable[[str], Path]
) -> None:
    CORE.config_path = component_config_path(fixture)
    res = load_config({})
    assert any(message in str(err) for err in res.errors), res.errors
