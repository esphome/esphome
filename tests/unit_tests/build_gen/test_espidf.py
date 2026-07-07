"""Tests for esphome.build_gen.espidf module."""

from __future__ import annotations

import json
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.esp32 import (
    KEY_COMPONENTS,
    KEY_ESP32,
    KEY_IDF_VERSION,
    KEY_PATH,
    KEY_REF,
    KEY_REPO,
)
import esphome.config_validation as cv
from esphome.const import KEY_CORE
from esphome.core import CORE


@pytest.fixture(autouse=True)
def _reset_core(tmp_path: Path) -> None:
    """Give each test its own CORE.build_path and a clean esp32 data slot."""
    CORE.build_path = str(tmp_path)
    CORE.data.setdefault(KEY_CORE, {})
    CORE.data[KEY_ESP32] = {
        KEY_COMPONENTS: {},
        KEY_IDF_VERSION: cv.Version(5, 5, 4),
    }


def _write_project_description(tmp_path: Path, components: dict[str, str]) -> None:
    """Stub a project_description.json with the given component_name -> dir map."""
    build_dir = tmp_path / "build"
    build_dir.mkdir(exist_ok=True)
    (build_dir / "project_description.json").write_text(
        json.dumps(
            {
                "build_component_info": {
                    name: {"dir": dir_} for name, dir_ in components.items()
                }
            }
        )
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


def test_get_available_components_filters_src_managed_and_pio(tmp_path: Path) -> None:
    """Built-ins are returned; src/, managed_components/, pio_components/ skipped."""
    _write_project_description(
        tmp_path,
        {
            "src": f"{tmp_path}/src",
            "esp_lcd": "/idf/components/esp_lcd",
            "espressif__arduino-esp32": f"{tmp_path}/managed_components/arduino",
            "JPEGDEC": f"{tmp_path}/pio_components/arduino/abc/bitbank2/JPEGDEC",
            "freertos": "/idf/components/freertos",
        },
    )
    from esphome.build_gen.espidf import get_available_components

    assert sorted(get_available_components()) == ["esp_lcd", "freertos"]


def test_get_project_cmakelists_minimal_omits_builtin_components_property(
    tmp_path: Path,
) -> None:
    """Minimal write must not emit ESPHOME_PROJECT_BUILTIN_COMPONENTS even
    when project_description.json exists (the data may be stale on the
    first write before the discovery pass refreshes it)."""
    _write_project_description(tmp_path, {"esp_lcd": "/idf/components/esp_lcd"})

    with (
        patch("esphome.build_gen.espidf.get_esp32_variant", return_value="ESP32"),
        patch.object(CORE, "name", "test"),
    ):
        from esphome.build_gen.espidf import get_project_cmakelists

        content = get_project_cmakelists(minimal=True)

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

    with (
        patch("esphome.build_gen.espidf.get_esp32_variant", return_value="ESP32"),
        patch.object(CORE, "name", "test"),
    ):
        from esphome.build_gen.espidf import get_project_cmakelists

        content = get_project_cmakelists(minimal=False)

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
