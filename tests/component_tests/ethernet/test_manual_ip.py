"""The manual IP table codegen emits must match the ETHERNET_MANUAL_IP layout in ethernet_component.h."""

from collections.abc import Callable
from pathlib import Path


def test_manual_ip_table_layout(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Twenty octets: static IP, gateway, subnet, dns1, dns2 (unset DNS is zero)."""
    main_cpp = generate_main(component_config_path("manual_ip.yaml"))

    expected = (
        "192, 168, 1, 50, 192, 168, 1, 1, 255, 255, 255, 0, 1, 1, 1, 1, 0, 0, 0, 0"
    )
    assert (
        f"esphome::ethernet::ETHERNET_MANUAL_IP[] PROGMEM = {{{expected}}};" in main_cpp
    )
    assert "set_manual_ip(" not in main_cpp


def test_no_manual_ip_emits_no_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("rmii_defaults.yaml"))

    assert "ETHERNET_MANUAL_IP" not in main_cpp
