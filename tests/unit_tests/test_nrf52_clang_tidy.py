"""Tests for esphome.components.nrf52.clang_tidy CMakeLists generation."""

import json
from pathlib import Path

import pytest

from esphome.components.nrf52.clang_tidy import (
    TIDY_PROJECT_NAME,
    _check_compile_commands_cover_sources,
    _tidy_cmakelists,
    root_path,
)
from esphome.core import EsphomeError


def test_tidy_cmakelists_includes_main_cpp_and_all_sources() -> None:
    content = _tidy_cmakelists("", ["a.cpp", "b.cpp"])

    assert '"main.cpp"' in content
    assert '"a.cpp"' in content
    assert '"b.cpp"' in content


def test_tidy_cmakelists_includes_library_include_dirs() -> None:
    content = _tidy_cmakelists('  "/some/library/include"', [])

    assert '"/some/library/include"' in content


def test_tidy_cmakelists_names_the_project() -> None:
    content = _tidy_cmakelists("", [])

    assert f"project({TIDY_PROJECT_NAME})" in content


def test_tidy_cmakelists_sets_expected_defines() -> None:
    content = _tidy_cmakelists("", [])

    for define in ("USE_ZEPHYR", "USE_NRF52", "CLANG_TIDY"):
        assert define in content


def test_tidy_cmakelists_includes_repo_root() -> None:
    content = _tidy_cmakelists("", [])

    assert f'"{root_path}"' in content


def _write_compile_commands(path: Path, files: list[str]) -> None:
    path.write_text(
        json.dumps(
            [{"directory": "/build", "command": "clang++ -c", "file": f} for f in files]
        ),
        encoding="utf-8",
    )


def test_check_compile_commands_cover_sources_passes_when_all_present(
    tmp_path: Path,
) -> None:
    a = str((tmp_path / "a.cpp").resolve())
    b = str((tmp_path / "b.cpp").resolve())
    compile_commands_path = tmp_path / "compile_commands.json"
    _write_compile_commands(compile_commands_path, [a, b, "/build/main.cpp"])

    _check_compile_commands_cover_sources(compile_commands_path, [a, b])


def test_check_compile_commands_cover_sources_raises_naming_dropped_source(
    tmp_path: Path,
) -> None:
    a = str((tmp_path / "a.cpp").resolve())
    b = str((tmp_path / "b.cpp").resolve())
    compile_commands_path = tmp_path / "compile_commands.json"
    # b.cpp silently dropped by CMake -- only a.cpp and the stub main.cpp got
    # a compile command.
    _write_compile_commands(compile_commands_path, [a, "/build/main.cpp"])

    with pytest.raises(EsphomeError, match="b.cpp"):
        _check_compile_commands_cover_sources(compile_commands_path, [a, b])
