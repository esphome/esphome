"""mDNS keeps only the predefined interfaces the config brings up, with one slot each."""

from collections.abc import Callable
from pathlib import Path

import pytest

from tests.component_tests.network import sdkconfig_option


@pytest.mark.parametrize(
    ("config_file", "sta", "ap", "eth", "slots"),
    [
        ("wifi_only.yaml", True, False, False, 2),
        ("wifi_ap.yaml", True, True, False, 2),
        ("ethernet_only.yaml", False, False, True, 2),
        ("wifi_ethernet.yaml", True, False, True, 2),
        ("wifi_ap_ethernet.yaml", True, True, True, 3),
    ],
)
def test_mdns_interface_slots(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    sta: bool,
    ap: bool,
    eth: bool,
    slots: int,
) -> None:
    generate_main(component_config_path(config_file))
    assert sdkconfig_option("CONFIG_MDNS_PREDEF_NETIF_STA") is sta
    assert sdkconfig_option("CONFIG_MDNS_PREDEF_NETIF_AP") is ap
    assert sdkconfig_option("CONFIG_MDNS_PREDEF_NETIF_ETH") is eth
    assert sdkconfig_option("CONFIG_MDNS_MAX_INTERFACES") == slots
