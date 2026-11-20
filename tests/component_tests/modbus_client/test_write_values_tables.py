"""Tests for modbus_client constant write values in shared PROGMEM tables."""

from collections.abc import Callable
from pathlib import Path
import re


def test_constant_values_share_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Equal value lists share one table across actions; lambdas stay templates."""
    main_cpp = generate_main(component_config_path("write_values_tables.yaml"))

    tables = re.findall(
        r"static constexpr (uint16_t|uint8_t) \w+\[\] PROGMEM = (\{[^}]*\});", main_cpp
    )
    # Coils pack LSB first: 0b01001101, then bit 8 in the second byte
    assert tables == [("uint16_t", "{0x1234, 0xABCD}"), ("uint8_t", "{77, 1}")]
    assert main_cpp.count("set_values_static(modbus_client_registers, 2);") == 2
    assert main_cpp.count("set_values_static(modbus_client_coils, 9);") == 2
    assert "set_values_template(" in main_cpp
