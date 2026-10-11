"""Tests for the template alarm control panel codes codegen."""

from collections.abc import Callable
from pathlib import Path


def test_codes_share_one_progmem_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Identical code lists share one nullptr-terminated flash table; empty lists set none."""
    main_cpp = generate_main(component_config_path("alarm_codes.yaml"))

    assert (
        'static constexpr const char * alarm_codes[] PROGMEM = {"1234", "5678", nullptr};'
        in main_cpp
    )
    assert "panel_a->set_codes(alarm_codes);" in main_cpp
    assert "panel_b->set_codes(alarm_codes);" in main_cpp
    assert "panel_none->set_codes(" not in main_cpp
