"""Constant IR byte codes live in shared PROGMEM tables; lambdas stay lambdas."""

from collections.abc import Callable
from pathlib import Path


def test_ir_codes_use_shared_progmem_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("ir_codes.yaml"))

    assert main_cpp.count("static constexpr uint8_t remote_base_code") == 2
    assert main_cpp.count("->set_code_static(remote_base_code, 5);") == 2
    assert "->set_data_static(remote_base_code_2, 6);" in main_cpp
    assert "->set_code_template([](" in main_cpp
    assert "std::vector<uint8_t>{" not in main_cpp
