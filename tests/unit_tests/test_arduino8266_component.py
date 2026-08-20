"""Tests for esphome.arduino8266.component (library resolution)."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.arduino8266 import component
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM, PLATFORM_ESP8266
from esphome.core import CORE, Library
from esphome.platformio.library import ConvertedLibrary, LibraryBackend


@pytest.fixture(autouse=True)
def _reset_libraries() -> None:
    CORE.platformio_libraries = {}
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP8266}


def _add_library(name: str, version: str | None, repository: str | None = None) -> None:
    CORE.add_library(Library(name=name, version=version, repository=repository))


def _make_framework(tmp_path: Path) -> Path:
    framework = tmp_path / "framework"
    lib = framework / "libraries" / "ESP8266WiFi" / "src"
    lib.mkdir(parents=True)
    (lib / "ESP8266WiFi.cpp").write_text("")
    (lib / "ESP8266WiFi.h").write_text("")
    (lib.parent / "library.properties").write_text("name=ESP8266WiFi\nversion=1.0\n")
    root_lib = framework / "libraries" / "Wire"
    root_lib.mkdir(parents=True)
    (root_lib / "Wire.cpp").write_text("")
    (root_lib / "examples").mkdir()
    (root_lib / "examples" / "scan.ino").write_text("")
    return framework


def test_library_info_src_layout(tmp_path: Path) -> None:
    framework = _make_framework(tmp_path)
    lib = component._bundled_library(framework, "ESP8266WiFi")
    assert lib.name == "ESP8266WiFi"
    assert [p.name for p in lib.sources] == ["ESP8266WiFi.cpp"]
    assert lib.include_dirs == [(framework / "libraries/ESP8266WiFi/src").resolve()]


def test_library_info_root_layout_excludes_examples(tmp_path: Path) -> None:
    framework = _make_framework(tmp_path)
    lib = component._bundled_library(framework, "Wire")
    assert [p.name for p in lib.sources] == ["Wire.cpp"]
    assert lib.include_dirs == [(framework / "libraries/Wire").resolve()]


def test_library_info_flags_parsing(tmp_path: Path) -> None:
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    (read_path / "src" / "a.cpp").write_text("")
    (read_path / "inc").mkdir()
    (read_path / "blobs").mkdir()
    data = {
        "build": {
            "flags": [
                "-DFOO=1 -I inc",
                "-lalgobsec",
                "-fno-lto",
                "-Wl,--wrap=malloc",
                "-l",
                "m",
                "-L",
                "blobs",
            ],
        }
    }
    lib = component._library_info("x", read_path, data)
    assert lib.flags == ["-DFOO=1", "-fno-lto"]
    assert lib.include_dirs == [
        (read_path / "src").resolve(),
        (read_path / "inc").resolve(),
    ]
    assert lib.link_dirs == [(read_path / "blobs").resolve()]
    assert lib.link_libs == ["algobsec", "m"]
    assert lib.link_flags == ["-Wl,--wrap=malloc"]


def test_library_info_no_src_dir(tmp_path: Path) -> None:
    read_path = tmp_path / "empty"
    read_path.mkdir()
    lib = component._library_info("x", read_path, {})
    # With no manifest hints the source dir falls back to the library root
    assert lib.sources == []
    assert lib.include_dirs == [read_path.resolve()]


def test_resolve_libraries_bundled(tmp_path: Path) -> None:
    framework = _make_framework(tmp_path)
    _add_library("ESP8266WiFi", None)
    libs = component.resolve_libraries(framework)
    assert [lib.name for lib in libs] == ["ESP8266WiFi"]


def test_resolve_libraries_bare_registry_name_is_external(tmp_path: Path) -> None:
    """A bare name that is not bundled resolves from the registry at the
    latest version, matching PlatformIO and the documented libraries: key."""
    framework = _make_framework(tmp_path)
    _add_library("pngle", None)
    with patch.object(component, "convert_libraries", return_value=[]) as mock_convert:
        component.resolve_libraries(framework)
    (libraries, _backend), _ = mock_convert.call_args
    assert [lib.name for lib in libraries] == ["pngle"]


def _converted(name: str, source_dir: Path, data: dict) -> ConvertedLibrary:
    converted = ConvertedLibrary(name, "1.0.0", source=None)
    converted.path = source_dir
    converted.data = data
    return converted


def test_resolve_libraries_external_and_bundled_deps(tmp_path: Path) -> None:
    framework = _make_framework(tmp_path)
    _add_library("ESP32Async/ESPAsyncWebServer", "3.9.6")

    lib_dir = tmp_path / "converted" / "webserver"
    (lib_dir / "src").mkdir(parents=True)
    (lib_dir / "src" / "server.cpp").write_text("")
    converted = _converted(
        "esp32async__ESPAsyncWebServer",
        lib_dir,
        {
            "build": {},
            "dependencies": [
                # Version-less bundled dependency: resolved from the framework
                {"name": "Wire", "platforms": "espressif8266"},
                # Wrong platform: skipped
                {"name": "ESP8266WiFi", "platforms": "espressif32"},
                # Registry dependency with a version: handled by the converter
                {"name": "ESPAsyncTCP", "owner": "ESP32Async", "version": "^2.0.0"},
                # Not bundled: skipped
                {"name": "NotBundled"},
            ],
        },
    )

    def fake_convert(libraries: list, backend: LibraryBackend) -> list:
        assert backend.platform == "espressif8266"
        assert backend.framework == "arduino"
        assert backend.cache_key == "arduino8266"
        backend.emit(converted)
        return [converted]

    with (
        patch.object(component, "convert_libraries", side_effect=fake_convert),
        patch.object(component, "apply_extra_script") as mock_extra,
    ):
        libs = component.resolve_libraries(framework)

    mock_extra.assert_called_once_with(
        converted, "esp8266", pio_platform="espressif8266"
    )
    assert [lib.name for lib in libs] == [
        "Wire",
        "esp32async__ESPAsyncWebServer",
    ]


def test_resolve_libraries_bundled_dep_already_present(tmp_path: Path) -> None:
    framework = _make_framework(tmp_path)
    _add_library("Wire", None)
    _add_library("Some/External", "1.0.0")

    lib_dir = tmp_path / "converted" / "external"
    lib_dir.mkdir(parents=True)
    converted = _converted(
        "some__External", lib_dir, {"dependencies": [{"name": "Wire"}]}
    )

    def fake_convert(libraries: list, backend: LibraryBackend) -> list:
        backend.emit(converted)
        return [converted]

    with (
        patch.object(component, "convert_libraries", side_effect=fake_convert),
        patch.object(component, "apply_extra_script"),
    ):
        libs = component.resolve_libraries(framework)

    # Wire appears once (from the explicit registration), not twice
    assert [lib.name for lib in libs] == ["Wire", "some__External"]


def test_resolve_libraries_versioned_bare_name_is_external(tmp_path: Path) -> None:
    """A bare name with a version pin ("pngle@1.1.0") is a registry package,
    not a bundled library, and must reach the converter."""
    framework = _make_framework(tmp_path)
    _add_library("pngle", "1.1.0")

    with patch.object(component, "convert_libraries", return_value=[]) as mock_convert:
        component.resolve_libraries(framework)

    (libraries, _backend), _ = mock_convert.call_args
    assert [lib.name for lib in libraries] == ["pngle"]


def test_library_info_trailing_bare_flag_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    lib = component._library_info("x", read_path, {"build": {"flags": ["-DA=1 -l"]}})
    assert lib.flags == ["-DA=1"]
    assert lib.link_libs == []
    assert "Ignoring trailing '-l'" in caplog.text


def test_library_info_missing_explicit_include_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    read_path = tmp_path / "lib"
    (read_path / "src").mkdir(parents=True)
    lib = component._library_info("x", read_path, {"build": {"flags": ["-Inope"]}})
    assert lib.include_dirs == [(read_path / "src").resolve()]
    assert "include dir nope which does not exist" in caplog.text


def test_library_info_missing_declared_dirs_warn(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Explicitly declared srcDir/includeDir that do not exist warn by name."""
    read_path = tmp_path / "lib"
    read_path.mkdir()
    component._library_info(
        "x", read_path, {"build": {"srcDir": "nosrc", "includeDir": "noinc"}}
    )
    assert "srcDir nosrc which does not exist" in caplog.text
    assert "include dir noinc which does not exist" in caplog.text


def test_resolve_libraries_lib_ignore_covers_bundled(tmp_path: Path) -> None:
    """lib_ignore applies to framework-bundled libraries, as under PlatformIO."""
    framework = _make_framework(tmp_path)
    _add_library("ESP8266WiFi", None)
    _add_library("Wire", None)
    CORE.platformio_options = {"lib_ignore": ["Wire"]}
    libs = component.resolve_libraries(framework)
    assert [lib.name for lib in libs] == ["ESP8266WiFi"]


def test_resolve_libraries_lib_ignore_covers_bundled_dependencies(
    tmp_path: Path,
) -> None:
    framework = _make_framework(tmp_path)
    _add_library("Some/External", "1.0.0")
    CORE.platformio_options = {"lib_ignore": ["Wire"]}

    lib_dir = tmp_path / "converted" / "external"
    lib_dir.mkdir(parents=True)
    converted = _converted(
        "some__External", lib_dir, {"dependencies": [{"name": "Wire"}]}
    )

    def fake_convert(libraries: list, backend: LibraryBackend) -> list:
        backend.emit(converted)
        return [converted]

    with (
        patch.object(component, "convert_libraries", side_effect=fake_convert),
        patch.object(component, "apply_extra_script"),
    ):
        libs = component.resolve_libraries(framework)

    assert [lib.name for lib in libs] == ["some__External"]
