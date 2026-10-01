"""Tests for esphome.espidf.clang_tidy tidy-project generation."""

# pylint: disable=protected-access

import json
import os
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import pytest
import yaml

from esphome.components.esp32.const import KEY_ESP32, KEY_IDF_VERSION
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_TARGET_FRAMEWORK, KEY_TARGET_PLATFORM
from esphome.core import CORE
from esphome.espidf import clang_tidy
from esphome.espidf.clang_tidy import (
    _arduino_excluded_stubs,
    _convert_pio_libs,
    _esphome_manifest_deps,
    _Settings,
    _setup_core,
    _write_tidy_project,
)
import esphome.espidf.component as espidf_component

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


def test_esphome_manifest_deps_reads_repo_manifest() -> None:
    """Top-level dependency names, independent of the per-dependency rules."""
    manifest = yaml.safe_load(
        (REPO_ROOT / "esphome" / "idf_component.yml").read_text(encoding="utf-8")
    )

    deps = _esphome_manifest_deps()

    assert isinstance(deps, set)
    assert "esphome/noise-c" in deps
    assert "esphome/libsodium" in deps
    # Cross-check against a fresh parse instead of hardcoding the manifest's
    # whole key list, so this doesn't need updating whenever a dependency is
    # added or removed.
    assert deps == set(manifest["dependencies"])


def test_convert_pio_libs_arduino_framework_passes_empty_managed(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Below IDF 6.0 the manifest gates noise-c/libsodium off on Arduino, so they
    still go through the converter."""
    _set_idf_version(cv.Version(5, 5, 5))
    monkeypatch.setattr(clang_tidy, "_parse_lib_deps", lambda ini, framework: [])

    captured: dict[str, set[str] | None] = {}

    # A converted library the batch resolves, so the loop wiring its
    # override_path into the returned deps mapping is exercised for real too.
    converted = SimpleNamespace(
        get_sanitized_name=lambda: "esphome/other-lib",
        path=tmp_path / "other-lib",
    )

    def fake_generate_idf_components(libraries, managed=None):
        captured["managed"] = managed
        return [converted]

    monkeypatch.setattr(
        espidf_component, "generate_idf_components", fake_generate_idf_components
    )

    result = _convert_pio_libs(tmp_path / "platformio.ini", "arduino")

    assert captured["managed"] == set()
    assert result == {
        "esphome/other-lib": {"override_path": str(tmp_path / "other-lib")}
    }


def test_convert_pio_libs_arduino_idf_6_passes_manifest_deps(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """From IDF 6.0 the manifest enables them on Arduino too, so the converter
    must skip them."""
    _set_idf_version(cv.Version(6, 0, 0))
    monkeypatch.setattr(clang_tidy, "_parse_lib_deps", lambda ini, framework: [])

    captured: dict[str, set[str] | None] = {}

    def fake_generate_idf_components(libraries, managed=None):
        captured["managed"] = managed
        return []

    monkeypatch.setattr(
        espidf_component, "generate_idf_components", fake_generate_idf_components
    )

    assert _convert_pio_libs(tmp_path / "platformio.ini", "arduino") == {}
    assert captured["managed"] == _esphome_manifest_deps()


def test_convert_pio_libs_espidf_framework_passes_manifest_deps(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Manifest-provided components are passed as ``managed`` so the converter
    skips them."""
    _set_idf_version(cv.Version(5, 5, 5), "espidf")
    monkeypatch.setattr(clang_tidy, "_parse_lib_deps", lambda ini, framework: [])

    captured: dict[str, set[str] | None] = {}

    def fake_generate_idf_components(libraries, managed=None):
        captured["managed"] = managed
        return []

    monkeypatch.setattr(
        espidf_component, "generate_idf_components", fake_generate_idf_components
    )

    result = _convert_pio_libs(tmp_path / "platformio.ini", "espidf")

    assert captured["managed"] == _esphome_manifest_deps()
    assert "esphome/noise-c" in captured["managed"]
    assert result == {}


def _set_idf_version(version: cv.Version, framework: str = "arduino") -> None:
    CORE.reset()
    CORE.data[KEY_ESP32] = {KEY_IDF_VERSION: version}
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: "esp32",
        KEY_TARGET_FRAMEWORK: framework,
    }


def test_arduino_excluded_stubs_skips_components_esphome_manifest_provides(
    tmp_path: Path,
) -> None:
    """A component ESPHome's own manifest declares (espressif/lan867x for ethernet)
    is not stubbed; one only arduino-esp32 bundles still is."""
    _set_idf_version(cv.Version(5, 5, 4))

    deps = _arduino_excluded_stubs(tmp_path)

    # lan867x is a real ESPHome dependency (esphome/idf_component.yml), so it
    # must be excluded from the stub set.
    assert "espressif/lan867x" not in deps
    # espressif/libsodium (arduino-esp32's bundled copy) is a different
    # package from ESPHome's own esphome/libsodium, so below IDF 6.0, where
    # arduino-esp32 still declares it, it is stubbed.
    assert "espressif/libsodium" in deps
    stub_info = deps["espressif/libsodium"]
    assert stub_info["version"] == "*"
    stub_path = Path(stub_info["override_path"])
    assert (stub_path / "CMakeLists.txt").is_file()


def test_arduino_excluded_stubs_skips_libsodium_from_idf_6(tmp_path: Path) -> None:
    """From IDF 6.0 arduino-esp32 drops espressif/libsodium; a stub would clash
    with esphome/libsodium."""
    _set_idf_version(cv.Version(6, 0, 0))

    deps = _arduino_excluded_stubs(tmp_path)

    assert "espressif/libsodium" not in deps
    # Other arduino-bundled components are still stubbed.
    assert "espressif/cbor" in deps
