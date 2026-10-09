"""Widgets whose LVGL C implementation creates or references labels
internally (tab titles, key legends, the QR canvas fallback) must declare
the label dependency in ``get_uses()``. Otherwise a config that contains
no ``label`` widget of its own compiles LVGL without ``LV_USE_LABEL`` and
fails at C compile time with undefined ``lv_label_*`` symbols.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.lvgl import defines as df


@pytest.mark.parametrize(
    "yaml_file",
    [
        "qrcode_no_label.yaml",
        "keyboard_no_label.yaml",
        "tabview_no_label.yaml",
    ],
)
def test_label_less_config_enables_lv_use_label(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    yaml_file: str,
) -> None:
    generate_main(component_config_path(yaml_file))
    assert "LV_USE_LABEL" in df.get_defines()
