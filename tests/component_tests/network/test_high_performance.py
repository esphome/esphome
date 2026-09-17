"""Tests for the ``network: enable_high_performance:`` lwip tier selection.

Ethernet drivers keep received frames in internal RAM, so any ethernet
interface must keep lwip off the PSRAM tier even when PSRAM is guaranteed.
"""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.network import require_high_performance_networking
from tests.component_tests.network import sdkconfig_option


@pytest.mark.parametrize(
    ("fixture", "window", "mailbox", "window_scale", "wifi_rx_buffers"),
    [
        ("high_perf_wifi_psram.yaml", 512000, 512, True, 512),
        ("high_perf_wifi_ethernet_psram.yaml", 32768, 64, None, 512),
        ("high_perf_ethernet_no_psram.yaml", 32768, 64, None, None),
    ],
)
def test_lwip_tier(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    fixture: str,
    window: int,
    mailbox: int,
    window_scale: bool | None,
    wifi_rx_buffers: int | None,
) -> None:
    # The wifi component only reacts to a component request, so request it the
    # way sendspin does; the config key alone decides the lwip tier.
    require_high_performance_networking()
    generate_main(component_config_path(fixture))
    assert sdkconfig_option("CONFIG_LWIP_TCP_WND_DEFAULT") == window
    assert sdkconfig_option("CONFIG_LWIP_TCP_RECVMBOX_SIZE") == mailbox
    assert sdkconfig_option("CONFIG_LWIP_TCPIP_RECVMBOX_SIZE") == mailbox
    assert sdkconfig_option("CONFIG_LWIP_WND_SCALE") is window_scale
    # Wifi RX buffers really go to PSRAM, so the wifi tier is never downgraded
    assert sdkconfig_option("CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM") == wifi_rx_buffers
