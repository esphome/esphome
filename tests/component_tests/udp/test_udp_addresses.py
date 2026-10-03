"""Tests for the udp component codegen."""

from collections.abc import Callable
from pathlib import Path


def test_addresses_are_a_progmem_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Addresses live in a nullptr-terminated flash table instead of a heap vector."""
    main_cpp = generate_main(component_config_path("udp_addresses.yaml"))

    assert (
        "static constexpr const char * udp_addresses[] PROGMEM = "
        '{"10.0.0.1", "10.0.0.2", nullptr};' in main_cpp
    )
    assert "->set_addresses(udp_addresses);" in main_cpp


def test_empty_addresses_emit_no_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """An empty address list leaves the pointer null instead of emitting a table."""
    main_cpp = generate_main(component_config_path("udp_no_addresses.yaml"))

    assert "udp_addresses" not in main_cpp
    assert "set_addresses(" not in main_cpp
