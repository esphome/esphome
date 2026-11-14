"""Unit tests for script/analyze_component_buses.py."""

from pathlib import Path
import sys

sys.path.insert(0, str((Path(__file__).parent / ".." / ".." / "script").resolve()))

import analyze_component_buses  # noqa: E402

# A uart bus written into the file itself instead of taken from a shared bus package
UART_BUS = "uart:\n  - id: uart_bus\n"
# A build test that uses no bus at all
NO_BUS = "logger:\n"


def _add_component(tests_dir: Path, name: str, files: dict[str, str]) -> None:
    component_dir = tests_dir / name
    component_dir.mkdir()
    for file_name, content in files.items():
        (component_dir / file_name).write_text(content, encoding="utf-8")


def test_bus_in_benchmark_yaml_keeps_component_groupable(tmp_path: Path) -> None:
    """A bus only in benchmark.yaml keeps a component groupable.

    benchmark.yaml only feeds the C++ unit test build, never the grouped
    build test. A bus in a build test still makes a component non-groupable.
    """
    _add_component(
        tmp_path,
        "bus_in_benchmark",
        {"test.esp32-idf.yaml": NO_BUS, "benchmark.yaml": UART_BUS},
    )
    _add_component(tmp_path, "bus_in_test", {"test.esp32-idf.yaml": UART_BUS})

    _, non_groupable, direct_bus = analyze_component_buses.analyze_all_components(
        tmp_path
    )

    assert non_groupable == {"bus_in_test"}
    assert direct_bus == {"bus_in_test"}
