"""Tests for esphome.espidf.clang_tidy tidy-project generation."""

import json
import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.espidf import clang_tidy
from esphome.espidf.clang_tidy import _Settings, _setup_core, _write_tidy_project

REPO_ROOT = Path(__file__).resolve().parents[2]


def _settings(idf_target: str = "esp32", target_framework: str = "espidf") -> _Settings:
    return _Settings(
        idf_target=idf_target,
        variant=idf_target.upper(),
        idf_version="5.5.4",
        target_framework=target_framework,
        platform_defines=(
            "USE_ESP32",
            f"USE_ESP32_VARIANT_{idf_target.upper()}",
            "USE_ESP_IDF",
        ),
        framework_deps={},
    )


def test_write_tidy_project_copies_base_sdkconfig(tmp_path: Path) -> None:
    """The shared sdkconfig.defaults is always copied; no per-target file for esp32."""
    _write_tidy_project(tmp_path, [], {}, _settings("esp32"))

    assert (tmp_path / "sdkconfig.defaults").is_file()
    # esp32 has no sdkconfig.defaults.esp32, so nothing extra is copied.
    assert not (tmp_path / "sdkconfig.defaults.esp32").exists()


def test_write_tidy_project_copies_per_target_sdkconfig(tmp_path: Path) -> None:
    """A repo-root sdkconfig.defaults.<target> is also copied into the build dir."""
    _write_tidy_project(tmp_path, [], {}, _settings("esp32c6"))

    target = tmp_path / "sdkconfig.defaults.esp32c6"
    assert (tmp_path / "sdkconfig.defaults").is_file()
    assert target.is_file()
    assert target.read_text(encoding="utf-8") == (
        REPO_ROOT / "sdkconfig.defaults.esp32c6"
    ).read_text(encoding="utf-8")


@pytest.mark.parametrize(
    ("target_framework", "expected"),
    [("arduino", "1"), ("espidf", "0")],
)
def test_setup_core_sets_arduino_env(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    target_framework: str,
    expected: str,
) -> None:
    """_setup_core sets ESPHOME_ARDUINO_COMPONENT, which gates arduino-only manifest deps."""
    # monkeypatch snapshots os.environ, so the env var _setup_core writes is
    # restored after the test instead of leaking into later tests.
    monkeypatch.delenv("ESPHOME_ARDUINO_COMPONENT", raising=False)

    _setup_core(tmp_path / "proj", _settings(target_framework=target_framework))

    assert os.environ["ESPHOME_ARDUINO_COMPONENT"] == expected


def test_idedata_from_tidy_project(tmp_path) -> None:
    """The tidy TU's compile entry is assembled into consumer-shaped idedata."""
    compile_commands = tmp_path / "compile_commands.json"
    compile_commands.write_text(
        json.dumps(
            [
                {
                    "directory": str(tmp_path),
                    "file": str(tmp_path / "main" / "tidy.cpp"),
                    "command": "/tc/xtensa-esp32-elf-g++ -DUSE_ESP32 "
                    f"-I{tmp_path}/inc -c main/tidy.cpp -o tidy.o",
                }
            ]
        )
    )
    with patch(
        "esphome.espidf.clang_tidy.get_toolchain_includes", return_value=["/tc/inc"]
    ):
        data = clang_tidy._idedata_from_tidy_project(compile_commands)
    assert data["cxx_path"] == "/tc/xtensa-esp32-elf-g++"
    assert data["defines"] == ["USE_ESP32"]
    assert data["includes"]["toolchain"] == ["/tc/inc"]
    assert any(inc.endswith("/inc") for inc in data["includes"]["build"])


def test_idedata_from_tidy_project_missing_tu_raises(tmp_path) -> None:
    compile_commands = tmp_path / "compile_commands.json"
    compile_commands.write_text(json.dumps([]))
    with pytest.raises(RuntimeError, match="tidy.cpp not found"):
        clang_tidy._idedata_from_tidy_project(compile_commands)
