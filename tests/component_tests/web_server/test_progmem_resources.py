"""Tests for the web_server resources emitted as flash tables."""

from collections.abc import Callable
import gzip
from pathlib import Path
import re


def _table(main_cpp: str, name: str) -> bytes:
    # Anchored so a static table does not match
    match = re.search(
        rf"^const uint8_t ESPHOME_WEBSERVER_{name}\[\] PROGMEM = \{{([^}}]*)\}};",
        main_cpp,
        re.MULTILINE,
    )
    assert match is not None
    return bytes(int(byte) for byte in match.group(1).split(","))


def test_resources_are_externally_linked_tables(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """web_server.cpp reads these through extern declarations, so they must not be static."""
    main_cpp = generate_main(component_config_path("includes.yaml"))

    assert b"<esp-app></esp-app>" in _table(main_cpp, "INDEX_HTML")
    assert gzip.decompress(_table(main_cpp, "CSS_INCLUDE")) == b"body { color: red; }\n"
    assert gzip.decompress(_table(main_cpp, "JS_INCLUDE")) == b'console.log("hi");\n'
    css_size = len(_table(main_cpp, "CSS_INCLUDE"))
    assert (
        f"constexpr size_t ESPHOME_WEBSERVER_CSS_INCLUDE_SIZE = {css_size};" in main_cpp
    )
