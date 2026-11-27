"""Constant IR byte codes are encoded from shared PROGMEM tables; lambdas are called inline."""

from collections.abc import Callable
from pathlib import Path
import re


def test_ir_codes_use_shared_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("ir_codes.yaml"))

    tables = re.findall(
        r"static constexpr uint8_t (remote_base_code\w*)\[\] PROGMEM", main_cpp
    )
    assert len(tables) == 4  # Midea (shared by two actions), AEHA, Haier, Mirage
    midea = tables[0]
    assert main_cpp.count(f"apply_call.data = remote_base::MideaData({midea}, 5);") == 2
    assert re.search(r"apply_call\.encode\(0x8008, remote_base_code\w*, 6\);", main_cpp)
    assert re.search(r"apply_call\.encode\(remote_base_code\w*, 13\);", main_cpp)
    assert re.search(r"apply_call\.encode\(remote_base_code\w*, 14\);", main_cpp)
    assert "apply_call.encode([]() -> std::vector<uint8_t> {" in main_cpp
    # Constant codes are read from the table, never copied into a vector
    assert "std::vector<uint8_t>{" not in main_cpp
