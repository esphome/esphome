"""Tests for the epaper_spi source file filtering."""

from collections.abc import Callable
from pathlib import Path
import re
from typing import Any

import pytest

from esphome.components import epaper_spi
from esphome.components.epaper_spi import (
    BASE_SOURCE_FILE,
    DOMAIN,
    FILTER_SOURCE_FILES,
    register_sources,
)
from esphome.components.epaper_spi.models import EpaperModel
from esphome.config import iter_components
from esphome.core import CORE

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
    "Set source_files on the model in esphome/components/epaper_spi/models/<...> to include "
    "its class's .cpp and every parent class's .cpp, excluding epaper_spi.cpp."
)

CLASS_RE = re.compile(r"^class\s+(\w+)\s+(?:final\s+)?:\s*public\s+(\w+)", re.MULTILINE)


def _class_table() -> dict[str, tuple[str, str]]:
    """Map each driver class to its header stem and parent class, read from the headers."""
    table: dict[str, tuple[str, str]] = {}
    for header in COMPONENT_DIR.glob("*.h"):
        for name, parent in CLASS_RE.findall(header.read_text()):
            table[name] = (header.stem, parent)
    return table


def _expected_sources(class_name: str, table: dict[str, tuple[str, str]]) -> set[str]:
    """The .cpp files of a class and its parents, up to but excluding epaper_spi.cpp."""
    sources: set[str] = set()
    current = class_name
    while current != "EPaperBase":
        assert current in table, (
            f"No 'class {current} : public <Parent>' declaration found in the headers "
            f"in {COMPONENT_DIR}. {FIX_HINT}"
        )
        stem, current = table[current]
        sources.add(f"{stem}.cpp")
    return sources - {BASE_SOURCE_FILE}


@pytest.mark.parametrize("model_name", sorted(EpaperModel.models))
def test_model_source_files_match_headers(model_name: str) -> None:
    """A model must list exactly the sources its class hierarchy needs."""
    model = EpaperModel.models[model_name]

    expected = _expected_sources(model.class_name, _class_table())

    assert expected == set(model.source_files), (
        f"Model {model_name} ({model.class_name}) needs {sorted(expected)} but lists "
        f"{sorted(model.source_files)}. {FIX_HINT}"
    )


def test_every_source_file_is_claimed_by_a_model() -> None:
    on_disk = {p.name for p in COMPONENT_DIR.glob("*.cpp")} - {BASE_SOURCE_FILE}
    claimed = {f for m in EpaperModel.models.values() for f in m.source_files}

    assert not on_disk - claimed, (
        f"Source files no model lists: {sorted(on_disk - claimed)}. {FIX_HINT}"
    )


def test_listed_source_files_exist() -> None:
    on_disk = {p.name for p in COMPONENT_DIR.glob("*.cpp")}
    listed = {f for m in EpaperModel.models.values() for f in m.source_files}

    assert not listed - on_disk, (
        f"Listed source files that do not exist: {sorted(listed - on_disk)}. {FIX_HINT}"
    )


def test_missing_class_declaration_is_reported() -> None:
    with pytest.raises(AssertionError, match="No 'class EPaperGone"):
        _expected_sources("EPaperGone", _class_table())


def test_extend_keeps_class_name_and_source_files() -> None:

    class _Model(EpaperModel):
        def __init__(self, name: str, **kwargs: Any) -> None:
            super().__init__(name, "EPaperX", **kwargs)

    base = _Model("test-extend-base", source_files=("x.cpp",))
    try:
        assert base.extend("test-extend-child").source_files == ("x.cpp",)
        assert EpaperModel.models["TEST-EXTEND-CHILD"].class_name == "EPaperX"
        override = base.extend("test-extend-over", source_files=("y.cpp",))
        assert override.source_files == ("y.cpp",)
    finally:
        for key in ("TEST-EXTEND-BASE", "TEST-EXTEND-CHILD", "TEST-EXTEND-OVER"):
            EpaperModel.models.pop(key, None)


def test_filter_excludes_nothing_when_no_sources_recorded() -> None:
    assert FILTER_SOURCE_FILES() == []


def test_filter_excludes_unrecorded_sources() -> None:
    register_sources(["epaper_spi_jd79660.cpp"])

    excluded = FILTER_SOURCE_FILES()

    assert excluded == sorted(excluded)
    assert "epaper_spi_jd79660.cpp" not in excluded
    assert BASE_SOURCE_FILE not in excluded
    assert "epaper_spi_mono.cpp" in excluded


def test_filter_data_lives_in_core_data() -> None:
    register_sources(["epaper_spi_mono.cpp"])

    assert CORE.data[DOMAIN].needed_sources == {"epaper_spi_mono.cpp"}


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
