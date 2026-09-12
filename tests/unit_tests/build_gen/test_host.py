"""Tests for esphome.build_gen.host (the host ninja generator)."""

from __future__ import annotations

import logging
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.arduino.library import ArduinoLibrary
from esphome.build_gen import host as build_gen
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM, PLATFORM_HOST
from esphome.core import CORE, EsphomeError, Library
from esphome.host.toolchain import HostCompilers

COMPILERS = HostCompilers(cc="/usr/bin/gcc", cxx="/usr/bin/g++")


@pytest.fixture(autouse=True)
def _core(tmp_path: Path) -> None:
    CORE.build_path = tmp_path
    CORE.name = "dev"
    CORE.cpp_standard = "gnu++20"
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_HOST}


def _make_src(tmp_path: Path, *names: str) -> Path:
    src = tmp_path / "src"
    for name in names:
        path = src / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("")
    return src


def _render(ccache: str | None = None) -> tuple[bool, str]:
    changed = build_gen.write_project(COMPILERS, ccache)
    ninja = CORE.build_path / ".pioenvs" / "dev" / "build.ninja"
    return changed, ninja.read_text()


@pytest.mark.parametrize(
    ("tokens", "compile_flags", "link_flags"),
    [
        (
            ["-DUSE_HOST", "-Iinc", "-Ufoo", "-Wall"],
            ["-DUSE_HOST", "-Iinc", "-Ufoo", "-Wall"],
            [],
        ),
        (["-std=gnu++20"], ["-std=gnu++20"], []),
        (
            ["-lssl", "-L/opt/lib", "-Wl,--gc-sections"],
            [],
            ["-lssl", "-L/opt/lib", "-Wl,--gc-sections"],
        ),
        # Both lines, as SCons routes unclassified flags
        (
            ["-g", "-O2", "-fsanitize=address", "-pthread", "--coverage"],
            ["-g", "-O2", "-fsanitize=address", "-pthread", "--coverage"],
            ["-g", "-O2", "-fsanitize=address", "-pthread", "--coverage"],
        ),
        # Two-token flags travel with their argument
        (
            ["-include", "pre.h", "-isystem", "/x"],
            ["-include", "pre.h", "-isystem", "/x"],
            [],
        ),
        (["-framework", "CoreFoundation"], [], ["-framework", "CoreFoundation"]),
    ],
)
def test_split_flags(
    tokens: list[str], compile_flags: list[str], link_flags: list[str]
) -> None:
    assert build_gen.split_flags(tokens) == (compile_flags, link_flags)


def test_split_flags_trailing_argument_flag() -> None:
    with pytest.raises(EsphomeError, match="trailing '-include' with no argument"):
        build_gen.split_flags(["-g", "-include"])


def test_flag_lists_route_the_standard() -> None:
    """cpp_standard wins for C++; C never sees a C++ standard."""
    CORE.build_flags = {
        "-DUSE_HOST",
        "-Iinc",
        "-Wl,-x",
        "-g",
        "-lssl",
        "-std=c++17",
        "-std=gnu17",
    }
    CORE.cxx_build_flags = {"-Wno-volatile"}
    cflags, cxxflags, link_flags = build_gen._flag_lists()
    assert cflags == ["-DUSE_HOST", "-Iinc", "-g", "-std=gnu17"]
    assert cxxflags == ["-std=gnu++20", "-DUSE_HOST", "-Iinc", "-g", "-Wno-volatile"]
    assert link_flags == ["-Wl,-x", "-g", "-lssl"]


def test_flag_lists_without_cpp_standard_keeps_user_std() -> None:
    CORE.cpp_standard = None
    CORE.build_flags = {"-std=c++17"}
    cflags, cxxflags, _link = build_gen._flag_lists()
    assert cflags == []
    assert cxxflags == ["-std=c++17"]


def test_flag_lists_apply_unflags(caplog: pytest.LogCaptureFixture) -> None:
    CORE.build_flags = {"-g", "-DUSE_HOST"}
    CORE.build_unflags = {"-g", "-Onope"}
    with caplog.at_level(logging.WARNING):
        cflags, cxxflags, link_flags = build_gen._flag_lists()
    assert "-g" not in cflags + cxxflags + link_flags
    assert "-DUSE_HOST" in cflags
    assert "build_unflags entries matched no build flag: -Onope" in caplog.text


def test_resolve_host_libraries_without_libraries() -> None:
    with patch("esphome.arduino.library.resolve_libraries") as resolve:
        assert build_gen._resolve_host_libraries() == []
    resolve.assert_not_called()


def test_resolve_host_libraries_is_framework_less() -> None:
    CORE.add_library(Library(name="lvgl/lvgl", version="9.5.0"))
    lib = ArduinoLibrary(name="lvgl")
    with patch(
        "esphome.arduino.library.resolve_libraries", return_value=[lib]
    ) as resolve:
        assert build_gen._resolve_host_libraries() == [lib]
    resolve.assert_called_once_with(
        None,
        pio_platform="native",
        board_mcu="host",
        cache_key="host",
        framework=None,
        manifest_optional=True,
    )


def test_write_project_requires_generated_sources(tmp_path: Path) -> None:
    with pytest.raises(EsphomeError, match="Generated source directory .* is missing"):
        build_gen.write_project(COMPILERS, None)
    _make_src(tmp_path, "esphome.h")
    with pytest.raises(EsphomeError, match="No source files found"):
        build_gen.write_project(COMPILERS, None)


def test_write_project_emits_every_source_kind(tmp_path: Path) -> None:
    src = _make_src(tmp_path, "main.cpp", "esphome/core/a.c", "x.S", "y.s", "h.h")
    CORE.build_flags = {"-DUSE_HOST", "-g"}
    changed, ninja = _render(ccache="/usr/bin/ccache")
    assert changed is True
    assert "cc = '/usr/bin/gcc'" in ninja
    assert "cxx = '/usr/bin/g++'" in ninja
    assert "ccache = '/usr/bin/ccache'" in ninja
    assert f"build obj/src/main.cpp.o: cxx {src / 'main.cpp'}" in ninja
    assert f"build obj/src/esphome/core/a.c.o: c {src / 'esphome/core/a.c'}" in ninja
    assert "build obj/src/x.S.o: aspp " in ninja
    assert "build obj/src/y.s.o: asm " in ninja
    assert "h.h" not in ninja
    assert f"cflags = -DUSE_HOST -g -I'{src}'" in ninja
    assert f"cxxflags = -std=gnu++20 -DUSE_HOST -g -I'{src}'" in ninja
    # Assembly gets the defines and includes only
    assert f"asflags = -DUSE_HOST -I'{src}'" in ninja
    assert "linkflags = -g\n" in ninja
    assert "libdirflags = \n" in ninja
    assert "libflags = \n" in ninja
    assert "rule ar" not in ninja
    assert (
        "build program: link obj/src/esphome/core/a.c.o obj/src/main.cpp.o "
        "obj/src/x.S.o obj/src/y.s.o | \n  archives = \ndefault program\n"
    ) in ninja
    # Unchanged content reports no change so the compile DB can be reused
    changed, _ = _render(ccache="/usr/bin/ccache")
    assert changed is False


def test_write_project_without_ccache(tmp_path: Path) -> None:
    _make_src(tmp_path, "main.cpp")
    _changed, ninja = _render()
    assert "ccache = \n" in ninja


def test_write_project_routes_user_link_flags(tmp_path: Path) -> None:
    _make_src(tmp_path, "main.cpp")
    CORE.build_flags = {"-L/opt/lib", "-lcrypto", "-Wl,-framework,Security"}
    _changed, ninja = _render()
    assert "linkflags = -Wl,-framework,Security\n" in ninja
    assert "libdirflags = -L'/opt/lib'\n" in ninja
    assert "libflags = -lcrypto\n" in ninja


def _libraries(tmp_path: Path) -> list[ArduinoLibrary]:
    lib_dir = tmp_path / "libs"
    for name in ("foo/src/a.cpp", "foo/src/sub/b.c", "bare/x.cpp"):
        path = lib_dir / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("")
    archived = ArduinoLibrary(
        name="foo",
        sources=[lib_dir / "foo/src/a.cpp", lib_dir / "foo/src/sub/b.c"],
        include_dirs=[lib_dir / "foo/src"],
        flags=["-DFOO=1"],
        link_dirs=[lib_dir / "foo/lib"],
        link_libs=["bar"],
        link_flags=["-Wl,--gc-sections"],
    )
    direct = ArduinoLibrary(
        name="bare", sources=[lib_dir / "bare/x.cpp"], lib_archive=False
    )
    header_only = ArduinoLibrary(name="hdr", include_dirs=[lib_dir / "hdr"])
    return [archived, direct, header_only]


def test_write_project_with_libraries(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    src = _make_src(tmp_path, "main.cpp")
    libs = _libraries(tmp_path)
    with (
        patch.object(build_gen, "_resolve_host_libraries", return_value=libs),
        patch.object(build_gen, "find_tool", return_value="/usr/bin/ar") as find_tool,
        patch.object(build_gen.sys, "platform", "linux"),
        caplog.at_level(logging.DEBUG),
    ):
        _changed, ninja = _render()
    find_tool.assert_called_once_with("AR", ("ar",))
    assert (
        "rule ar\n  command = $python $buildtool ar '/usr/bin/ar' $out $out.rsp"
        in ninja
    )
    lib_dir = tmp_path / "libs"
    # Every library's include dir joins the global include path
    assert f"-I'{src}' -I'{lib_dir / 'foo/src'}' -I'{lib_dir / 'hdr'}'" in ninja
    assert "linkflags = -Wl,--gc-sections\n" in ninja
    assert f"libdirflags = -L'{lib_dir / 'foo/lib'}'\n" in ninja
    assert "libflags = -lbar\n" in ninja
    # Library sources compile with the library's own flags, rooted at their
    # common parent
    assert (
        f"build obj/lib/foo/a.cpp.o: cxx {lib_dir / 'foo/src/a.cpp'}\n  flags = -DFOO=1\n"
        in ninja
    )
    assert (
        f"build obj/lib/foo/sub/b.c.o: c {lib_dir / 'foo/src/sub/b.c'}\n  flags = -DFOO=1\n"
        in ninja
    )
    assert "build libfoo.a: ar obj/lib/foo/a.cpp.o obj/lib/foo/sub/b.c.o\n" in ninja
    # libArchive: false objects link directly; the archive is an order-only
    # input wrapped in a group for GNU ld
    assert (
        "build program: link obj/src/main.cpp.o obj/lib/bare/x.cpp.o | libfoo.a\n"
        "  archives = -Wl,--start-group libfoo.a -Wl,--end-group\n"
    ) in ninja
    assert "Library hdr has no source files" in caplog.text


def test_write_project_darwin_links_archives_bare(tmp_path: Path) -> None:
    _make_src(tmp_path, "main.cpp")
    libs = _libraries(tmp_path)
    with (
        patch.object(build_gen, "_resolve_host_libraries", return_value=libs),
        patch.object(build_gen, "find_tool", return_value="/usr/bin/ar"),
        patch.object(build_gen.sys, "platform", "darwin"),
    ):
        _changed, ninja = _render()
    assert "  archives = libfoo.a\n" in ninja
    assert "--start-group" not in ninja
