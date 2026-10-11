"""Tests for source-file-to-component attribution in memory analyzer."""

from unittest.mock import patch

from esphome.analyze_memory import MemoryAnalyzer


def _make_analyzer(external_components: set[str] | None = None) -> MemoryAnalyzer:
    """Create a MemoryAnalyzer with mocked dependencies."""
    with patch.object(MemoryAnalyzer, "__init__", lambda self, *a, **kw: None):
        analyzer = MemoryAnalyzer.__new__(MemoryAnalyzer)
        analyzer.external_components = external_components or set()
        analyzer._lib_hash_to_name = {}
    return analyzer


def test_source_file_to_component_main_cpp_relative() -> None:
    """ESPHome-generated src/main.cpp.o (nm path form) attributes to core."""
    analyzer = _make_analyzer()
    assert analyzer._source_file_to_component("src/main.cpp.o") == "[esphome]core"


def test_source_file_to_component_main_cpp_pioenvs_path() -> None:
    """Linker map paths like .pioenvs/<env>/src/main.cpp.o attribute to core."""
    analyzer = _make_analyzer()
    result = analyzer._source_file_to_component(".pioenvs/drivewaygate/src/main.cpp.o")
    assert result == "[esphome]core"


def test_source_file_to_component_esphome_core() -> None:
    """Sources under src/esphome/core/ attribute to core."""
    analyzer = _make_analyzer()
    result = analyzer._source_file_to_component("src/esphome/core/application.cpp.o")
    assert result == "[esphome]core"


def test_source_file_to_component_known_component() -> None:
    """Known ESPHome components attribute to their component name."""
    analyzer = _make_analyzer()
    result = analyzer._source_file_to_component(
        "src/esphome/components/wifi/wifi_component.cpp.o"
    )
    assert result == "[esphome]wifi"
