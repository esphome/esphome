"""Tests for esphome.espidf.clang_tidy tidy-project generation."""

# pylint: disable=protected-access

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


def test_espidf_only_deps_are_tsdb_engines() -> None:
    """The idf envs get tsdb's registry engines, pinned as tsdb pins them."""
    from esphome.components.tsdb import (
        ESP_LITTLEFS_COMPONENT,
        ESP_LITTLEFS_VERSION,
        ESP_TSDB_COMPONENT,
        ESP_TSDB_VERSION,
    )

    assert clang_tidy._espidf_only_deps() == {
        ESP_TSDB_COMPONENT: {"version": ESP_TSDB_VERSION},
        ESP_LITTLEFS_COMPONENT: {"version": ESP_LITTLEFS_VERSION},
    }


@pytest.mark.parametrize(
    ("target_framework", "expects_engines"),
    [("espidf", True), ("arduino", False)],
)
def test_generate_compile_commands_scopes_engines_to_idf(
    tmp_path: Path, target_framework: str, expects_engines: bool
) -> None:
    """Only the idf envs get tsdb's engines; the arduino envs get the stubs.

    littlefs is one of the components the arduino envs stub out, so the real
    dependency cannot be added alongside that stub.
    """
    written: list[dict] = []

    with (
        patch.object(clang_tidy, "_setup_core"),
        patch.object(clang_tidy, "_convert_pio_libs", return_value={}),
        patch.object(
            clang_tidy, "_arduino_excluded_stubs", return_value={"stub/x": {}}
        ),
        patch.object(
            clang_tidy,
            "_write_tidy_project",
            side_effect=lambda *a: written.append(a[2]),
        ),
        patch("esphome.espidf.toolchain.run_reconfigure", return_value=0),
        patch("esphome.build_gen.espidf.get_available_components", return_value=[]),
    ):
        clang_tidy._generate_compile_commands(
            tmp_path, _settings(target_framework=target_framework), tmp_path / "pio.ini"
        )

    # Both phases of the two-phase configure see the same deps.
    assert len(written) == 2
    engines = set(clang_tidy._espidf_only_deps())
    for deps in written:
        if expects_engines:
            assert engines <= set(deps)
            assert "stub/x" not in deps
        else:
            assert not engines & set(deps)
            assert "stub/x" in deps


def test_write_tidy_project_merges_espidf_only_deps(tmp_path: Path) -> None:
    """Those engines land in the written manifest as plain (unruled) deps.

    A rule that the tidy project's IDF version doesn't satisfy would leave tsdb
    without its engine headers again, which is what the manifest entries did.
    """
    import yaml

    from esphome.components.tsdb import ESP_TSDB_COMPONENT, ESP_TSDB_VERSION

    _write_tidy_project(tmp_path, [], clang_tidy._espidf_only_deps(), _settings())

    manifest = yaml.safe_load(
        (tmp_path / "main" / "idf_component.yml").read_text(encoding="utf-8")
    )
    deps = manifest["dependencies"]
    assert deps[ESP_TSDB_COMPONENT] == {"version": ESP_TSDB_VERSION}
    # ESPHome's own dependencies survive the merge.
    assert "bblanchon/arduinojson" in deps


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


@pytest.mark.parametrize(
    ("reconfigure_rcs", "error"),
    [
        ((1,), "ESP-IDF CMake configure \\(discovery\\) failed"),
        ((0, 1), "ESP-IDF CMake configure failed"),
        ((0, 0), None),
    ],
    ids=["discovery", "full", "ok"],
)
def test_generate_compile_commands_configures_twice(
    tmp_path: Path, reconfigure_rcs: tuple[int, ...], error: str | None
) -> None:
    """Discovery configure, then a configure requiring what it found."""
    with (
        patch.object(clang_tidy, "_setup_core"),
        patch.object(clang_tidy, "_convert_pio_libs", return_value={}),
        patch.object(clang_tidy, "_write_tidy_project") as mock_write,
        patch("esphome.espidf.toolchain.run_reconfigure", side_effect=reconfigure_rcs),
        patch(
            "esphome.build_gen.espidf.get_available_components",
            return_value=["lwip", "esp_timer"],
        ),
    ):
        if error:
            with pytest.raises(RuntimeError, match=error):
                clang_tidy._generate_compile_commands(
                    tmp_path, _settings(), tmp_path / "platformio.ini"
                )
            return
        result = clang_tidy._generate_compile_commands(
            tmp_path, _settings(), tmp_path / "platformio.ini"
        )
    assert result == tmp_path / "build" / "compile_commands.json"
    assert mock_write.call_args_list[1].args[1] == ["esp_timer", "lwip"]
