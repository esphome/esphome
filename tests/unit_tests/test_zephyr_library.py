"""Tests for the Zephyr backend of the shared PlatformIO library converter."""

from pathlib import Path

import pytest

import esphome.components.zephyr.library as zlib
from esphome.components.zephyr.library import (
    generate_cmakelists_txt,
    generate_module_yml,
    generate_zephyr_modules,
)
from esphome.core import EsphomeError, Library
from esphome.platformio.library import ConvertedLibrary, URLSource


def _make_component(path: Path, name: str = "mylib") -> ConvertedLibrary:
    c = ConvertedLibrary(name, "1.0", source=URLSource("http://dummy"))
    c.path = path
    return c


def test_generate_module_yml_uses_sanitized_name():
    c = ConvertedLibrary("owner/My Lib", "1.0", source=URLSource("http://dummy"))
    out = generate_module_yml(c)
    # "/" -> "__" and " " -> "_" so it's a valid Zephyr module name.
    assert "name: owner__My_Lib" in out
    assert "cmake: zephyr" in out


def test_generate_cmakelists_txt_basic(tmp_path):
    c = _make_component(tmp_path)
    src = tmp_path / "src"
    src.mkdir()
    (src / "main.c").write_text("int main() {}")
    c.data = {}

    out = generate_cmakelists_txt(c)

    assert "zephyr_library_named(mylib)" in out
    assert "zephyr_library_sources(" in out
    # Sources are emitted as absolute paths (CMakeLists lives in zephyr/ subdir),
    # backslash-escaped for CMake (matching the output on Windows).
    assert str((src / "main.c").resolve()).replace("\\", "\\\\") in out


def test_generate_cmakelists_txt_flags_and_includes(tmp_path):
    c = _make_component(tmp_path)
    (tmp_path / "src").mkdir()
    (tmp_path / "src" / "a.c").write_text("")
    (tmp_path / "include").mkdir()
    c.data = {"build": {"flags": ["-Iinclude", "-DFOO", "-Wall", "-Llibdir", "-lm"]}}

    out = generate_cmakelists_txt(c)

    assert "zephyr_include_directories(" in out
    assert str((tmp_path / "include").resolve()).replace("\\", "\\\\") in out
    assert "zephyr_library_compile_options(" in out
    assert "-DFOO" in out
    assert "-Wall" in out
    assert "zephyr_link_libraries(" in out
    assert "-Llibdir" in out
    assert "-lm" in out


def test_generate_zephyr_modules_collects_all_dirs_and_writes(tmp_path, monkeypatch):
    # Two converted libraries: one top-level, one transitive dependency. The
    # converter calls backend.emit for both; generate_zephyr_modules must return
    # *all* module dirs (not just top-level) so every module is discoverable.
    top = _make_component(tmp_path / "top", "top")
    (top.path / "src").mkdir(parents=True)
    (top.path / "src" / "t.c").write_text("")
    dep = _make_component(tmp_path / "dep", "dep")
    (dep.path / "src").mkdir(parents=True)
    (dep.path / "src" / "d.c").write_text("")

    captured = {}

    def fake_convert(libraries, backend):
        captured["platform"] = backend.platform
        captured["framework"] = backend.framework
        backend.emit(top)
        backend.emit(dep)
        return [top]

    monkeypatch.setattr(zlib, "convert_libraries", fake_convert)

    dirs = generate_zephyr_modules([Library("top", "1.0", None)])

    assert dirs == [top.path, dep.path]
    # Platform check disabled for Zephyr; framework declared as zephyr.
    assert captured["platform"] is None
    assert captured["framework"] == "zephyr"
    for comp in (top, dep):
        assert (comp.path / "zephyr" / "module.yml").is_file()
        assert (comp.path / "zephyr" / "CMakeLists.txt").is_file()


def test_generate_zephyr_modules_errors_on_duplicate_module_name(tmp_path, monkeypatch):
    # The same library referenced under inconsistent specs (e.g. bare vs
    # owner-qualified, or git vs registry) resolves to two components with the
    # same Zephyr module name, which would collide in zephyr_library_named().
    a = _make_component(tmp_path / "a", "esphome/noise-c")
    a.path.mkdir(parents=True)
    b = _make_component(tmp_path / "b", "esphome/noise-c")
    b.path.mkdir(parents=True)
    assert a.get_require_name() == b.get_require_name()

    def fake_convert(libraries, backend):
        backend.emit(a)
        backend.emit(b)
        return [a]

    monkeypatch.setattr(zlib, "convert_libraries", fake_convert)

    with pytest.raises(EsphomeError, match="same Zephyr module"):
        generate_zephyr_modules([Library("esphome/noise-c", "1.0", None)])
