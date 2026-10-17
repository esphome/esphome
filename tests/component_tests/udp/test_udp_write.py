"""Tests for the udp.write action codegen."""

from collections.abc import Callable
from pathlib import Path


def test_write_payloads_share_a_progmem_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Identical constant payloads share one flash table; an empty one needs none."""
    main_cpp = generate_main(component_config_path("udp_write.yaml"))

    assert main_cpp.count("udp_data[] PROGMEM = {0xAA, 0x55, 0x01, 0x02};") == 1
    assert main_cpp.count("->set_data_static(udp_data, 4);") == 2
    assert "->set_data_static(nullptr, 0);" in main_cpp
    assert "->set_data_template(" in main_cpp
