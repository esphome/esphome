"""Tests for the API server's generated subscribe and dump functions for imported entities."""

from collections.abc import Callable
from pathlib import Path
import re

from esphome.core import CORE

DIR = Path(__file__).parent

IMPORTS = (
    "ha_sensor_1",
    "ha_sensor_2",
    "ha_binary_sensor",
    "ha_text_sensor",
    "ha_switch",
    "ha_select",
    "ha_number",
    "ha_text",
)


def _body(main_cpp: str, name: str) -> str:
    match = re.search(rf"static void {name}\(\) \{{\n(.*?)\n\}}", main_cpp, re.S)
    assert match, name
    return match.group(1)


def test_imports_subscribed_and_dumped_by_api_server(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Every imported entity is subscribed and dumped exactly once; button and light are not."""
    main_cpp = generate_main(DIR / "mixed_entities.yaml")
    subscribe = _body(main_cpp, "esphome__api_apiserver_id__homeassistant_subscribe_fn")
    dump = _body(main_cpp, "esphome__api_apiserver_id__homeassistant_dump_fn")
    for var in IMPORTS:
        assert subscribe.count(f"::{var}->subscribe();") == 1, var
        assert dump.count(f"::{var}->dump_config();") == 1, var
        assert f"App.register_component_({var}" not in main_cpp
    for var in ("ha_button", "homeassistant_homeassistantlight_id"):
        assert var not in subscribe
        assert var not in dump
    assert "esphome__api_apiserver_id__homeassistant_subscribe_fn);" in main_cpp
    assert "esphome__api_apiserver_id__homeassistant_dump_fn);" in main_cpp
    assert any(d.name == "USE_API_HOMEASSISTANT_IMPORTS" for d in CORE.defines)


def test_button_only_has_no_imports(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A button alone imports nothing, so the API server's import hooks are compiled out."""
    main_cpp = generate_main(DIR / "button_valid.yaml")
    assert "homeassistant_subscribe_fn" not in main_cpp
    assert not any(d.name == "USE_API_HOMEASSISTANT_IMPORTS" for d in CORE.defines)
