"""Tests for the epaper_spi source file filtering."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import epaper_spi
from esphome.components.epaper_spi import (
    BASE_SOURCE_FILE,
    CLASS_SOURCES,
    DOMAIN,
    FILTER_SOURCE_FILES,
    register_class,
)
from esphome.components.epaper_spi.models import EpaperModel
from esphome.config import iter_components
from esphome.core import CORE, EsphomeError

COMPONENT_DIR = Path(epaper_spi.__file__).parent

TWO_MODELS_YAML = """
esphome:
  name: test

esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3

psram:
  mode: octal
  speed: 80MHz

spi:
  clk_pin: GPIO7
  mosi_pin: GPIO9

display:
  - platform: epaper_spi
    id: display_a
    model: seeed-reterminal-sticky-gray4
  - platform: epaper_spi
    id: display_b
    model: seeed-reterminal-e1001
"""


def _resource_names() -> set[str]:
    """Names of the epaper_spi sources the build would copy, merged over all manifests."""
    return {
        res.resource
        for _, component in iter_components(CORE.config)
        for res in component.resources
        if res.package == "esphome.components.epaper_spi"
    }


def _cpp_names(names: set[str]) -> set[str]:
    return {name for name in names if name.endswith(".cpp")}


FIX_HINT = (
    "Update CLASS_SOURCES in esphome/components/epaper_spi/__init__.py: every driver "
    "class needs an entry mapping it to its .cpp file and the class it derives from "
    "(None for EPaperBase), so the build keeps the files it needs."
)


def test_table_covers_every_model_and_source() -> None:
    """A new model or driver file must not be silently dropped from the build."""
    missing_classes = sorted(
        {
            model.class_name
            for model in EpaperModel.models.values()
            if model.class_name not in CLASS_SOURCES
        }
    )
    assert not missing_classes, f"Classes with no entry: {missing_classes}. {FIX_HINT}"

    missing_parents = sorted(
        {
            parent
            for _, parent in CLASS_SOURCES.values()
            if parent is not None and parent not in CLASS_SOURCES
        }
    )
    assert not missing_parents, (
        f"Parent classes with no entry: {missing_parents}. {FIX_HINT}"
    )

    table_files = [source for source, _ in CLASS_SOURCES.values()]
    duplicates = sorted({f for f in table_files if table_files.count(f) > 1})
    assert not duplicates, f"Files listed more than once: {duplicates}. {FIX_HINT}"

    on_disk = {p.name for p in COMPONENT_DIR.glob("*.cpp")} - {BASE_SOURCE_FILE}
    unlisted = sorted(on_disk - set(table_files))
    assert not unlisted, f"Source files with no entry: {unlisted}. {FIX_HINT}"
    stale = sorted(set(table_files) - on_disk)
    assert not stale, f"Entries for files that do not exist: {stale}. {FIX_HINT}"


def test_table_is_sorted() -> None:
    names = list(CLASS_SOURCES)
    assert names == sorted(names, key=str.lower), (
        "Keep CLASS_SOURCES in esphome/components/epaper_spi/__init__.py in "
        "alphabetical order of class name, ignoring case."
    )


def test_filter_excludes_nothing_when_no_class_recorded() -> None:
    assert FILTER_SOURCE_FILES() == []


def test_filter_keeps_parent_chain() -> None:
    register_class("EPaperSSD1677Gray4")

    excluded = set(FILTER_SOURCE_FILES())

    kept = {s for s, _ in CLASS_SOURCES.values()} - excluded
    assert kept == {
        "epaper_spi_ssd1677_gray4.cpp",
        "epaper_spi_ssd1677.cpp",
        "epaper_spi_mono.cpp",
    }
    assert BASE_SOURCE_FILE not in excluded


def test_filter_keeps_directly_derived_class_only() -> None:
    register_class("EPaperJD79660")

    excluded = set(FILTER_SOURCE_FILES())

    assert "epaper_spi_jd79660.cpp" not in excluded
    assert "epaper_spi_mono.cpp" in excluded


def test_filter_data_lives_in_core_data() -> None:
    register_class("EPaperMono")

    assert CORE.data[DOMAIN].needed_sources == {"epaper_spi_mono.cpp"}


def test_register_unknown_class_explains_fix() -> None:
    with pytest.raises(EsphomeError, match="missing from CLASS_SOURCES"):
        register_class("EPaperNotInTable")


def test_register_unknown_parent_explains_fix(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setitem(CLASS_SOURCES, "EPaperOrphan", ("orphan.cpp", "EPaperGone"))

    with pytest.raises(EsphomeError, match="'EPaperGone' is missing"):
        register_class("EPaperOrphan")


def test_sources_filtered_via_code_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The real to_code records the model, and every manifest honours the filter."""
    generate_main(component_config_path("ssd1677_gray4_test.yaml"))

    assert _cpp_names(_resource_names()) == {
        BASE_SOURCE_FILE,
        "epaper_spi_ssd1677_gray4.cpp",
        "epaper_spi_ssd1677.cpp",
        "epaper_spi_mono.cpp",
    }


def test_sources_filtered_for_two_models(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
) -> None:
    config_path = tmp_path / "two_models.yaml"
    config_path.write_text(TWO_MODELS_YAML)

    generate_main(config_path)

    assert _cpp_names(_resource_names()) == {
        BASE_SOURCE_FILE,
        "epaper_spi_ssd1677_gray4.cpp",
        "epaper_spi_ssd1677.cpp",
        "epaper_spi_mono.cpp",
        "epaper_spi_ultrachip.cpp",
        "epaper_spi_uc8179.cpp",
    }


@pytest.mark.parametrize("class_name", sorted(CLASS_SOURCES))
def test_every_class_resolves(class_name: str) -> None:
    register_class(class_name)

    excluded = set(FILTER_SOURCE_FILES())

    assert CLASS_SOURCES[class_name][0] not in excluded
