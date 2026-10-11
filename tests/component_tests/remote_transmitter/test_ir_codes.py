"""Constant IR byte codes live in shared PROGMEM tables; lambdas stay lambdas."""

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
    assert main_cpp.count(f"->set_code_static({midea}, 5);") == 2
    assert re.search(r"->set_data_static\(remote_base_code\w*, 6\);", main_cpp)
    assert re.search(r"->set_code_static\(remote_base_code\w*, 13\);", main_cpp)
    assert re.search(r"->set_code_static\(remote_base_code\w*, 14\);", main_cpp)
    assert "->set_code_template([](" in main_cpp
    assert "std::vector<uint8_t>{" not in main_cpp
