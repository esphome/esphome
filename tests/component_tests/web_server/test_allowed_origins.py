"""Tests for the web_server allowed_origins codegen."""

from collections.abc import Callable
from pathlib import Path


def test_allowed_origins_are_a_progmem_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Allowed origins live in a nullptr-terminated flash table instead of a heap vector."""
    main_cpp = generate_main(component_config_path("allowed_origins.yaml"))

    assert (
        "static constexpr const char * web_server_allowed_origins[] PROGMEM = "
        '{"https://app.esphome.io", "*", nullptr};' in main_cpp
    )
    assert "->set_allowed_origins(web_server_allowed_origins);" in main_cpp
