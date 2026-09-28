"""Tests for esphome.build_gen.espidf module."""

from __future__ import annotations

import json
import logging
import os
from pathlib import Path
import subprocess
from unittest.mock import patch

import pytest

from esphome.components.esp32 import (
    KEY_COMPONENTS,
    KEY_ESP32,
    KEY_EXCLUDE_COMPONENTS,
    KEY_IDF_VERSION,
    KEY_PATH,
    KEY_REF,
    KEY_REPO,
    register_exclude_components_cmake_arg,
)
import esphome.config_validation as cv
from esphome.const import KEY_CORE
from esphome.core import CORE, EsphomeError


@pytest.fixture(autouse=True)
def _reset_core(tmp_path: Path) -> None:
    """Give each test its own CORE.build_path and a clean esp32 data slot."""
    CORE.build_path = str(tmp_path)
    CORE.data.setdefault(KEY_CORE, {})
    CORE.data[KEY_ESP32] = {
        KEY_COMPONENTS: {},
        KEY_EXCLUDE_COMPONENTS: set(),
        KEY_IDF_VERSION: cv.Version(5, 5, 4),
    }


def _write_project_description(
    tmp_path: Path, components: dict[str, str], idf_path: str = "/idf"
) -> None:
    """Stub a project_description.json with the given component_name -> dir map."""
    build_dir = tmp_path / "build"
    build_dir.mkdir(exist_ok=True)
    (build_dir / "project_description.json").write_text(
        json.dumps(
            {
                "idf_path": idf_path,
                "build_component_info": {
                    name: {"dir": dir_} for name, dir_ in components.items()
                },
            }
        )
    )


def _render(minimal: bool = False, builtin_components: list[str] | None = None) -> str:
    """Render the top-level CMakeLists with the standard variant/name patches."""
    with (
        patch("esphome.build_gen.espidf.get_esp32_variant", return_value="ESP32"),
        patch.object(CORE, "name", "test"),
    ):
        from esphome.build_gen.espidf import get_project_cmakelists

        return get_project_cmakelists(
            minimal=minimal, builtin_components=builtin_components
        )


def test_get_available_components_returns_none_without_build_path() -> None:
    """No build_path set yet: must not raise on Path(None)."""
    CORE.build_path = None
    from esphome.build_gen.espidf import get_available_components

    assert get_available_components() is None


def test_get_available_components_returns_none_without_project_description(
    tmp_path: Path,
) -> None:
    from esphome.build_gen.espidf import get_available_components

    assert get_available_components() is None


def test_get_available_components_keeps_only_idf_tree_components(
    tmp_path: Path,
) -> None:
    """Only components under idf_path/components are built-ins: src, managed,
    converted PIO libs and Arduino component_stubs are all left out."""
    _write_project_description(
        tmp_path,
        {
            "src": f"{tmp_path}/src",
            "esp_lcd": "/idf/components/esp_lcd",
            "espressif__arduino-esp32": f"{tmp_path}/managed_components/arduino",
            "JPEGDEC": f"{tmp_path}/pio_components/arduino/abc/bitbank2/JPEGDEC",
            "cbor": f"{tmp_path}/component_stubs/cbor",
            "freertos": "/idf/components/freertos",
        },
    )
    from esphome.build_gen.espidf import get_available_components

    assert sorted(get_available_components()) == ["esp_lcd", "freertos"]


def test_codegen_and_configure_writes_render_the_same_cmakelists(
    tmp_path: Path,
) -> None:
    """write_project() at codegen time (no list) and the configure-time write
    (discovered list) must agree, or ninja re-runs cmake on every build."""
    _write_project_description(
        tmp_path,
        {
            "lwip": "/idf/components/lwip",
            "cbor": f"{tmp_path}/component_stubs/cbor",
        },
    )
    from esphome.build_gen.espidf import get_available_components

    assert _render() == _render(builtin_components=get_available_components())
    assert "ESPHOME_PROJECT_BUILTIN_COMPONENTS cbor" not in _render()


def test_get_available_components_warns_when_nothing_is_under_idf_path(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    _write_project_description(tmp_path, {"cbor": f"{tmp_path}/component_stubs/cbor"})
    from esphome.build_gen.espidf import (
        get_available_components,
        has_discovered_components,
    )

    assert get_available_components() == []
    assert "No ESP-IDF components found under" in caplog.text
    # An empty discovery must not count as configured, or it would be latched in.
    assert not has_discovered_components()


def test_get_available_components_ignores_corrupt_or_unexpected_file(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    from esphome.build_gen.espidf import (
        get_available_components,
        has_discovered_components,
    )

    (build_dir / "project_description.json").write_text("{not json")
    assert get_available_components() is None
    assert not has_discovered_components()
    (build_dir / "project_description.json").write_text('{"build_component_info": {}}')
    with caplog.at_level(logging.DEBUG, logger="esphome.build_gen.espidf"):
        assert get_available_components() is None
    assert "Could not read" in caplog.text


def test_has_discovered_components_after_configure(tmp_path: Path) -> None:
    _write_project_description(tmp_path, {"lwip": "/idf/components/lwip"})
    from esphome.build_gen.espidf import has_discovered_components

    assert has_discovered_components()


def test_get_project_cmakelists_size_command_uses_json2() -> None:
    """The POST_BUILD size command uses the cheap json2 format, with --ng
    only on the 1.x tool bundled with IDF < 6."""
    content = _render()
    assert "-m esp_idf_size --ng --format=json2" in content

    CORE.data[KEY_ESP32][KEY_IDF_VERSION] = cv.Version(6, 0, 0)
    content = _render()
    assert "--ng" not in content
    assert "--format=json2" in content


def test_get_project_cmakelists_uses_supplied_builtin_components() -> None:
    """A cached list replaces project_description.json and is still filtered
    by EXCLUDE_COMPONENTS."""
    with patch.dict(CORE.cmake_args, {"EXCLUDE_COMPONENTS": "fatfs;unity"}):
        content = _render(builtin_components=["lwip", "fatfs", "esp_timer"])
    assert "ESPHOME_PROJECT_BUILTIN_COMPONENTS esp_timer APPEND" in content
    assert "ESPHOME_PROJECT_BUILTIN_COMPONENTS lwip APPEND" in content
    assert "ESPHOME_PROJECT_BUILTIN_COMPONENTS fatfs APPEND" not in content


def test_get_project_cmakelists_minimal_omits_builtin_components_property(
    tmp_path: Path,
) -> None:
    """Minimal write must not emit ESPHOME_PROJECT_BUILTIN_COMPONENTS even
    when project_description.json exists (the data may be stale on the
    first write before the discovery pass refreshes it)."""
    _write_project_description(tmp_path, {"esp_lcd": "/idf/components/esp_lcd"})

    content = _render(minimal=True)

    assert "ESPHOME_PROJECT_BUILTIN_COMPONENTS" not in content


def test_get_project_cmakelists_full_emits_builtin_components_property(
    tmp_path: Path,
) -> None:
    """Non-minimal write emits one idf_build_set_property line per built-in,
    sorted, and excludes src/managed/pio components."""
    _write_project_description(
        tmp_path,
        {
            "src": f"{tmp_path}/src",
            "esp_lcd": "/idf/components/esp_lcd",
            "freertos": "/idf/components/freertos",
            "espressif__esp-dsp": f"{tmp_path}/managed_components/esp-dsp",
            "JPEGDEC": f"{tmp_path}/pio_components/arduino/abc/bitbank2/JPEGDEC",
        },
    )

    content = _render()

    assert (
        "idf_build_set_property(ESPHOME_PROJECT_BUILTIN_COMPONENTS esp_lcd APPEND)"
        in content
    )
    assert (
        "idf_build_set_property(ESPHOME_PROJECT_BUILTIN_COMPONENTS freertos APPEND)"
        in content
    )
    # Excluded by get_available_components filtering.
    assert "espressif__esp-dsp APPEND" not in content
    assert "JPEGDEC APPEND" not in content


def test_get_project_cmakelists_emits_cmake_args() -> None:
    """Args registered via CORE.add_cmake_arg() are emitted as set() lines,
    on minimal writes too."""
    CORE.add_cmake_arg("EXECUTABLE_COMPONENT_NAME", "src")

    content = _render(minimal=True)

    assert 'set(EXECUTABLE_COMPONENT_NAME "src")' in content


def test_get_project_cmakelists_escapes_backslashes_in_cmake_args() -> None:
    """Backslashes (the only character escaping applies to; the rest are
    rejected at registration) are doubled so CMake reads the value back
    verbatim."""
    CORE.add_cmake_arg("MY_PATH", r"C:\esp\idf")

    content = _render(minimal=True)

    assert r'set(MY_PATH "C:\\esp\\idf")' in content


def test_get_project_cmakelists_emits_exclude_components(tmp_path: Path) -> None:
    """Excluded components are passed to IDF via EXCLUDE_COMPONENTS and are
    dropped from ESPHOME_PROJECT_BUILTIN_COMPONENTS even when a stale
    project_description.json still lists them (requiring an excluded
    component would pull it back into the build)."""
    _write_project_description(
        tmp_path,
        {
            "esp_lcd": "/idf/components/esp_lcd",
            "freertos": "/idf/components/freertos",
            "unity": "/idf/components/unity",
        },
    )
    CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS] = {"unity", "esp_lcd"}
    register_exclude_components_cmake_arg()

    content = _render()

    assert 'set(EXCLUDE_COMPONENTS "esp_lcd;unity")' in content
    # Must be set before project() so project.cmake sees it.
    assert content.index("set(EXCLUDE_COMPONENTS") < content.index("project(test)")
    assert (
        "idf_build_set_property(ESPHOME_PROJECT_BUILTIN_COMPONENTS freertos APPEND)"
        in content
    )
    assert "ESPHOME_PROJECT_BUILTIN_COMPONENTS unity" not in content
    assert "ESPHOME_PROJECT_BUILTIN_COMPONENTS esp_lcd" not in content


def test_get_project_cmakelists_minimal_emits_exclude_components() -> None:
    """The discovery (minimal) write also excludes components so they never
    register in project_description.json."""
    CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS] = {"unity"}
    register_exclude_components_cmake_arg()

    content = _render(minimal=True)

    assert 'set(EXCLUDE_COMPONENTS "unity")' in content


def test_get_project_cmakelists_no_exclude_components_line_when_empty() -> None:
    """No EXCLUDE_COMPONENTS line at all when nothing is excluded."""
    register_exclude_components_cmake_arg()

    content = _render()

    assert "EXCLUDE_COMPONENTS" not in content


def test_include_builtin_idf_component_removes_exclusion() -> None:
    """include_builtin_idf_component() drops a name from the exclusion set so
    a component a config actually uses is not passed to EXCLUDE_COMPONENTS."""
    from esphome.components.esp32 import (
        exclude_builtin_idf_component,
        get_excluded_builtin_components,
        include_builtin_idf_component,
    )

    exclude_builtin_idf_component("esp_eth")
    exclude_builtin_idf_component("unity")
    include_builtin_idf_component("esp_eth")

    assert get_excluded_builtin_components() == ["unity"]

    register_exclude_components_cmake_arg()
    content = _render()

    assert 'set(EXCLUDE_COMPONENTS "unity")' in content
    assert "esp_eth" not in content


def test_write_project_writes_exclude_components_stamp(tmp_path: Path) -> None:
    """write_project() snapshots the exclusion set; the toolchain watches the
    stamp to trigger a discovery reconfigure when the set changes (excluded
    components never register in project_description.json)."""
    CORE.build_flags = set()
    CORE.build_path = tmp_path
    CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS] = {"unity", "esp_lcd"}

    with (
        patch("esphome.build_gen.espidf.get_esp32_variant", return_value="ESP32"),
        patch.object(CORE, "name", "test"),
    ):
        from esphome.build_gen.espidf import write_project

        write_project()

    stamp = tmp_path / "exclude_components.esphomeinternal"
    assert stamp.read_text() == "esp_lcd;unity"


def test_get_component_cmakelists_no_link_flags() -> None:
    """With no -Wl, flags the target_link_options block is emitted with an empty body."""
    CORE.build_flags = set()
    from esphome.build_gen.espidf import get_component_cmakelists

    content = get_component_cmakelists()
    assert "target_link_options(${COMPONENT_LIB} PUBLIC\n    \n)" in content


def test_get_component_cmakelists_single_link_flag() -> None:
    """A single -Wl, flag appears indented inside target_link_options."""
    CORE.build_flags = {"-Wl,--gc-sections"}
    from esphome.build_gen.espidf import get_component_cmakelists

    content = get_component_cmakelists()
    assert (
        "target_link_options(${COMPONENT_LIB} PUBLIC\n    -Wl,--gc-sections\n)"
        in content
    )


def test_get_component_cmakelists_multiple_link_flags_sorted() -> None:
    """Multiple -Wl, flags are sorted and joined with the four-space indent."""
    CORE.build_flags = {"-Wl,-z,noexecstack", "-Wl,--gc-sections", "-Wl,-Map=out.map"}
    from esphome.build_gen.espidf import get_component_cmakelists

    content = get_component_cmakelists()
    expected = (
        "target_link_options(${COMPONENT_LIB} PUBLIC\n"
        "    -Wl,--gc-sections\n"
        "    -Wl,-Map=out.map\n"
        "    -Wl,-z,noexecstack\n"
        ")"
    )
    assert expected in content


def test_get_component_cmakelists_compile_flags_excluded_from_link_opts() -> None:
    """-D and -W (non-linker) flags must not appear in target_link_options."""
    CORE.build_flags = {"-DFOO", "-Wall", "-Wl,--gc-sections"}
    from esphome.build_gen.espidf import get_component_cmakelists

    content = get_component_cmakelists()
    assert "-DFOO" not in content.split("target_link_options")[1]
    assert "-Wall" not in content.split("target_link_options")[1]
    assert "-Wl,--gc-sections" in content


def test_get_component_cmakelists_globs_alternate_cpp_extensions() -> None:
    """Both app_sources glob variants include .cc/.cxx/.c++ so vendored sources
    are compiled, matching the extensions PlatformIO's builder globs by default."""
    CORE.build_flags = set()
    from esphome.build_gen.espidf import get_component_cmakelists

    content = get_component_cmakelists()
    for ext in ("cc", "cxx", "c++"):
        assert content.count(f'"${{CMAKE_CURRENT_SOURCE_DIR}}/*.{ext}"') == 2
        assert content.count(f'"${{CMAKE_CURRENT_SOURCE_DIR}}/esphome/*.{ext}"') == 2


def test_get_project_cmakelists_emits_managed_components_property(
    tmp_path: Path,
) -> None:
    """ESPHOME_PROJECT_MANAGED_COMPONENTS is always emitted (both modes)
    from the esp32 add_idf_component registry."""
    CORE.data[KEY_ESP32][KEY_COMPONENTS] = {
        "espressif/esp-dsp": {KEY_REPO: None, KEY_REF: "1.7.1", KEY_PATH: None},
        "espressif/arduino-esp32": {KEY_REPO: None, KEY_REF: "3.3.8", KEY_PATH: None},
    }

    with (
        patch("esphome.build_gen.espidf.get_esp32_variant", return_value="ESP32"),
        patch.object(CORE, "name", "test"),
    ):
        from esphome.build_gen.espidf import get_project_cmakelists

        for minimal in (True, False):
            content = get_project_cmakelists(minimal=minimal)
            assert (
                "idf_build_set_property(ESPHOME_PROJECT_MANAGED_COMPONENTS"
                " espressif__arduino-esp32 APPEND)"
            ) in content
            assert (
                "idf_build_set_property(ESPHOME_PROJECT_MANAGED_COMPONENTS"
                " espressif__esp-dsp APPEND)"
            ) in content


def test_get_project_cmakelists_replaces_cpp_standard(tmp_path: Path) -> None:
    """cg.set_cpp_standard() replaces the IDF default -std in
    CXX_COMPILE_OPTIONS between include(project.cmake) and project()."""
    with (
        patch("esphome.build_gen.espidf.get_esp32_variant", return_value="ESP32"),
        patch.object(CORE, "name", "test"),
        patch.object(CORE, "cpp_standard", "gnu++20"),
    ):
        from esphome.build_gen.espidf import get_project_cmakelists

        content = get_project_cmakelists(minimal=True)

    assert (
        "idf_build_get_property(esphome_cxx_compile_options CXX_COMPILE_OPTIONS)"
        in content
    )
    assert 'list(FILTER esphome_cxx_compile_options EXCLUDE REGEX "^-std=")' in content
    assert 'list(APPEND esphome_cxx_compile_options "-std=gnu++20")' in content
    # The replacement must come after project.cmake (which appends the IDF
    # default) and before project() (which consumes the options).
    include_pos = content.index("tools/cmake/project.cmake")
    replace_pos = content.index("CXX_COMPILE_OPTIONS")
    project_pos = content.index("project(test)")
    assert include_pos < replace_pos < project_pos


def test_get_project_cmakelists_no_cpp_standard(tmp_path: Path) -> None:
    with (
        patch("esphome.build_gen.espidf.get_esp32_variant", return_value="ESP32"),
        patch.object(CORE, "name", "test"),
        patch.object(CORE, "cpp_standard", None),
        patch.object(CORE, "cxx_build_flags", set()),
    ):
        from esphome.build_gen.espidf import get_project_cmakelists

        content = get_project_cmakelists(minimal=True)

    assert "CXX_COMPILE_OPTIONS" not in content


def test_get_project_cmakelists_cxx_build_flags(tmp_path: Path) -> None:
    """Flags registered via cg.add_cxx_build_flag() are appended to
    CXX_COMPILE_OPTIONS (C++-only, GCC warns if they reach C compiles)
    between include(project.cmake) and project()."""
    with (
        patch("esphome.build_gen.espidf.get_esp32_variant", return_value="ESP32"),
        patch.object(CORE, "name", "test"),
        patch.object(CORE, "cpp_standard", None),
        patch.object(CORE, "cxx_build_flags", {"-Wno-volatile"}),
    ):
        from esphome.build_gen.espidf import get_project_cmakelists

        content = get_project_cmakelists(minimal=True)

    flag_line = 'idf_build_set_property(CXX_COMPILE_OPTIONS "-Wno-volatile" APPEND)'
    assert flag_line in content
    include_pos = content.index("tools/cmake/project.cmake")
    flag_pos = content.index(flag_line)
    project_pos = content.index("project(test)")
    assert include_pos < flag_pos < project_pos


def test_get_component_cmakelists_no_compile_features() -> None:
    """The C++ standard is pinned project-wide via CXX_COMPILE_OPTIONS in the
    top-level CMakeLists; the src component must not set its own."""
    with patch.object(CORE, "build_flags", set()):
        from esphome.build_gen.espidf import get_component_cmakelists

        content = get_component_cmakelists()

    assert "target_compile_features" not in content


def _make_pch_device(tmp_path: Path, name: str) -> Path:
    """A device dir with the pch source headers and a stub compile_commands."""
    from esphome.build_helpers.pch import PCH_DEFAULT_HEADERS

    dev = tmp_path / name
    for header in PCH_DEFAULT_HEADERS:
        path = dev / "src" / header
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "esphome/core/defines.h"\n')
    # A real quoted include chain and a per-device-named sdkconfig with
    # identical content: the closure and sdkconfig inputs must be exercised
    (dev / "src" / "esphome" / "core" / "defines.h").write_text(
        '#include "esphome/core/macros.h"\n'
    )
    (dev / "src" / "esphome" / "core" / "macros.h").write_text("#define M 1\n")
    # Both spellings: tests patch CORE.name to "test" or to the device name
    (dev / f"sdkconfig.{name}").write_text("CONFIG_X=y\n")
    (dev / "sdkconfig.test").write_text("CONFIG_X=y\n")
    build = dev / "build"
    build.mkdir(exist_ok=True)
    from esphome.build_helpers.pch import write_pch_headers

    write_pch_headers(build, PCH_DEFAULT_HEADERS)
    src_file = str(dev / "src" / "esphome" / "a.cpp")
    _write_db(
        build,
        "g++ -DX=1 -include esphome_pch.h "
        f'-o esp-idf/src/CMakeFiles/__idf_src.dir/a.cpp.obj -c "{src_file}"',
        src_file,
    )
    return dev


def _prepare(dev: Path, name: str = "test", returncode: int = 0) -> str:
    """Run prepare_pch with a stub compiler; return the .sum text."""
    from esphome.build_gen.espidf import prepare_pch

    CORE.build_path = dev

    def compile_(cmd, **kwargs):
        # The compile must target the include list, not the guard header
        assert cmd[-5:-2] == [
            "c++-header",
            "-c",
            str(dev / "build" / "esphome_pch_src.h"),
        ]
        if returncode == 0:
            (dev / "build" / "esphome_pch.h.gch").write_bytes(b"gch")
        return subprocess.CompletedProcess(cmd, returncode, "", "boom")

    with (
        patch.object(CORE, "name", name),
        patch("esphome.build_helpers.pch.subprocess.run", side_effect=compile_),
    ):
        prepare_pch()
    return (dev / "build" / "esphome_pch.h.gch.sum").read_text()


def _write_db(build: Path, command: str, file: str) -> None:
    (build / "compile_commands.json").write_text(
        json.dumps([{"directory": str(build), "command": command, "file": file}])
    )


def test_prepare_pch_writes_header_and_sum(tmp_path: Path) -> None:
    from esphome.build_gen.espidf import prepare_pch

    dev = _make_pch_device(tmp_path, "dev_a")
    assert len(_prepare(dev).strip()) == 64
    # A compiler that skips the .gch reads this header: it must be an error
    assert "#error" in (dev / "build" / "esphome_pch.h").read_text()
    # Unchanged inputs: the second call must not recompile
    with (
        patch.object(CORE, "name", "test"),
        patch("esphome.build_helpers.pch.subprocess.run", side_effect=AssertionError),
    ):
        prepare_pch()


@pytest.mark.parametrize("user_basedir", [False, True])
def test_pch_no_device_path_in_flags_or_sum(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, user_basedir: bool
) -> None:
    """The per-device build path in either would stop ccache sharing
    between devices."""
    from esphome.build_gen.espidf import get_component_cmakelists

    if user_basedir:
        # A parent of the build path must not shadow it
        monkeypatch.setenv("CCACHE_BASEDIR", str(tmp_path))
    sums = []
    for name in ("dev_a", "dev_b"):
        dev = _make_pch_device(tmp_path, name)
        sums.append(_prepare(dev, name))
        assert str(dev) not in get_component_cmakelists()
    assert sums[0] == sums[1]


def test_pch_compile_command_variants(tmp_path: Path) -> None:
    """Missing DB, no matching entry, and launcher-prefixed commands."""
    from esphome.build_helpers.pch import pch_compile_command

    build = tmp_path / "build"
    build.mkdir()
    header = build / "esphome_pch.h"
    gch = build / "esphome_pch.h.gch"
    with pytest.raises(FileNotFoundError):
        pch_compile_command(build, header, gch)

    _write_db(build, "gcc -c other.c", "other.c")
    with pytest.raises(EsphomeError, match="has no ESPHome"):
        pch_compile_command(build, header, gch)

    src_file = str(tmp_path / "src" / "esphome" / "a.cpp")
    _write_db(
        build,
        "/usr/bin/ccache g++ -DX=1 -include user.h -include esphome_pch.h -MMD "
        "-MT a.cpp.obj -MF a.cpp.obj.d "
        f"-o esp-idf/src/CMakeFiles/__idf_src.dir/a.cpp.obj -c {src_file}",
        src_file,
    )
    # Launcher, the pch -include, -o/-c and depfile flags removed
    cmd, cmd_dir = pch_compile_command(build, header, gch)
    assert cmd == [
        "g++",
        "-DX=1",
        "-include",
        "user.h",
        "-x",
        "c++-header",
        "-c",
        str(header),
        "-o",
        str(gch),
    ]
    # The compile must run where the flags were resolved
    assert cmd_dir == build


def test_component_cmakelists_pch_block(monkeypatch: pytest.MonkeyPatch) -> None:
    from esphome.build_gen.espidf import get_component_cmakelists

    content = get_component_cmakelists()
    assert '"$<$<COMPILE_LANGUAGE:CXX>:-include>"' in content
    assert '"$<$<COMPILE_LANGUAGE:CXX>:esphome_pch.h>"' in content
    assert 'OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/esphome_pch.h"' in content
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    assert "-include" not in get_component_cmakelists()


def test_prepare_pch_compile_failure_stops_the_build(tmp_path: Path) -> None:
    dev = _make_pch_device(tmp_path, "dev_f")
    with pytest.raises(EsphomeError, match="ESPHOME_PCH_ENABLE=0.*: boom"):
        _prepare(dev, returncode=1)
    assert not (dev / "build" / "esphome_pch.h.gch.sum").exists()


def test_prepare_pch_missing_sdkconfig_stops_the_build(tmp_path: Path) -> None:
    dev = _make_pch_device(tmp_path, "dev_m")
    (dev / "sdkconfig.test").unlink()
    with pytest.raises(EsphomeError, match="prepare the precompiled header"):
        _prepare(dev)


def test_prepare_pch_broken_compile_database_stops_the_build(tmp_path: Path) -> None:
    dev = _make_pch_device(tmp_path, "dev_j")
    (dev / "build" / "compile_commands.json").write_text("[{")
    with pytest.raises(EsphomeError, match="ESPHOME_PCH_ENABLE=0"):
        _prepare(dev)


def test_prepare_pch_disabled_does_nothing(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from esphome.build_gen.espidf import prepare_pch

    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    CORE.build_path = tmp_path / "nothing"
    with patch("esphome.build_helpers.pch.subprocess.run", side_effect=AssertionError):
        prepare_pch()
    assert not (tmp_path / "nothing").exists()


def test_prepare_pch_bumps_header_for_object_depends(tmp_path: Path) -> None:
    """Consumers depend on the header, so a rebuilt .gch must bump it."""
    dev = _make_pch_device(tmp_path, "dev_t")
    header = dev / "build" / "esphome_pch.h"
    os.utime(header, (0, 0))
    _prepare(dev)
    assert header.stat().st_mtime > 0


def test_prepare_pch_command_change_invalidates_sum(tmp_path: Path) -> None:
    """A flag-only change in the compile DB must rebuild the .gch."""
    dev = _make_pch_device(tmp_path, "dev_c")
    first = _prepare(dev)
    db = dev / "build" / "compile_commands.json"
    db.write_text(db.read_text().replace("-DX=1", "-DX=2"))
    assert _prepare(dev) != first
