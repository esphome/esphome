"""Tests for the wireguard allowed IPs codegen."""

from collections.abc import Callable
from pathlib import Path


def test_allowed_ips_are_a_progmem_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Allowed IPs live in a flash table ended by an empty entry instead of a heap vector."""
    main_cpp = generate_main(component_config_path("allowed_ips.yaml"))

    table = main_cpp.split(
        "wireguard::AllowedIP wireguard_allowed_ips[] PROGMEM = ", 1
    )[1]
    table = table.split(";", 1)[0]
    assert '.ip = "172.16.34.0"' in table
    assert '.netmask = "255.255.0.0"' in table
    assert ".ip = nullptr" in table
    assert "->set_allowed_ips(wireguard_allowed_ips);" in main_cpp
