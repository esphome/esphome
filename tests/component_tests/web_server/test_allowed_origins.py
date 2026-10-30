"""Tests for the web_server allowed_origins codegen."""

from collections.abc import Callable
from pathlib import Path
import re

from esphome.core import CORE


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def test_allowed_origins_are_a_progmem_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Allowed origins live in a nullptr-terminated flash table instead of a heap vector."""
    main_cpp = generate_main(component_config_path("allowed_origins.yaml"))

    assert (
        "static constexpr const char * web_server_allowed_origins[] PROGMEM = "
        '{"https://app.esphome.io", "http://other.local:8080", nullptr};' in main_cpp
    )
    assert "USE_WEBSERVER_ALLOWED_ORIGINS" in _defines()
    assert "USE_WEBSERVER_ALLOW_ANY_ORIGIN" not in _defines()
    # Set on web_server_base, whose middleware checks every handler added with add_handler()
    assert re.search(
        r"\bweb_server_base_\w+->set_allowed_origins\(web_server_allowed_origins\);",
        main_cpp,
    )


def test_wildcard_origin_stores_no_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A "*" entry allows any origin at build time, so no table or list check is generated."""
    main_cpp = generate_main(component_config_path("allowed_origins_wildcard.yaml"))

    assert "USE_WEBSERVER_ALLOW_ANY_ORIGIN" in _defines()
    assert "USE_WEBSERVER_ALLOWED_ORIGINS" not in _defines()
    assert "web_server_allowed_origins" not in main_cpp
    assert "set_allowed_origins" not in main_cpp
