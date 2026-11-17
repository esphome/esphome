"""Tests for source-file-to-component attribution in memory analyzer."""

import pytest

from . import make_analyzer


def test_source_file_to_component_main_cpp_relative() -> None:
    """ESPHome-generated src/main.cpp.o (nm path form) attributes to core."""
    analyzer = make_analyzer()
    assert analyzer._source_file_to_component("src/main.cpp.o") == "[esphome]core"


def test_source_file_to_component_main_cpp_pioenvs_path() -> None:
    """Linker map paths like .pioenvs/<env>/src/main.cpp.o attribute to core."""
    analyzer = make_analyzer()
    result = analyzer._source_file_to_component(".pioenvs/drivewaygate/src/main.cpp.o")
    assert result == "[esphome]core"


def test_source_file_to_component_esphome_core() -> None:
    """Sources under src/esphome/core/ attribute to core."""
    analyzer = make_analyzer()
    result = analyzer._source_file_to_component("src/esphome/core/application.cpp.o")
    assert result == "[esphome]core"


def test_source_file_to_component_known_component() -> None:
    """Known ESPHome components attribute to their component name."""
    analyzer = make_analyzer()
    result = analyzer._source_file_to_component(
        "src/esphome/components/wifi/wifi_component.cpp.o"
    )
    assert result == "[esphome]wifi"


@pytest.mark.parametrize(
    ("rel_path", "component"),
    [
        (
            "lib/esp32async__ESPAsyncWebServer/WebHandlers.cpp.o",
            "[lib]espasyncwebserver",
        ),
        ("lib/ESP8266WiFi/ESP8266WiFi.cpp.o", "[lib]esp8266wifi"),
        ("core/core_esp8266_main.cpp.o", "arduino_core"),
    ],
)
def test_native_library_objects(rel_path: str, component: str) -> None:
    """Objects of the native ESP8266 build's libraries and core get a component."""
    assert make_analyzer()._source_file_to_component(rel_path) == component
