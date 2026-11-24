"""Tests for the code generated for an LVGL widget's ``on_boot``."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from tests.component_tests.helpers import callback_function, constructed, function_body


def test_widget_on_boot_runs_at_priority_390(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A widget's on_boot is a StartupCallback at 390.0f (0x43C30000), after the widgets exist."""
    main_cpp = generate_main(component_config_path("widget_on_boot.yaml"))
    function = callback_function(main_cpp, "boot_label_boot")
    assert constructed(main_cpp, "boot_label_boot") == (
        f"StartupCallback<{function}, 0x43C30000>()"
    )
    assert "label booted" in function_body(main_cpp, function)
