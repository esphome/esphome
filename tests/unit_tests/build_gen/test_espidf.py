"""Tests for esphome.build_gen.espidf module."""

from __future__ import annotations

import json
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.esp32 import (
    KEY_COMPONENTS,
    KEY_ESP32,
    KEY_PATH,
    KEY_REF,
    KEY_REPO,
)
from esphome.const import KEY_CORE
from esphome.core import CORE


@pytest.fixture(autouse=True)
def _reset_core(tmp_path: Path) -> None:
    """Give each test its own CORE.build_path and a clean esp32 data slot."""
    CORE.build_path = str(tmp_path)
    CORE.data.setdefault(KEY_CORE, {})
    CORE.data[KEY_ESP32] = {KEY_COMPONENTS: {}}


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
