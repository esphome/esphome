"""Validate per-display API metadata with the normal component schema and ID pass."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize(
    ("depth", "byte_order", "transform", "readback"),
    [
        (16, "little_endian", True, True),
        (16, "big_endian", False, True),
        (24, "little_endian", False, False),
    ],
)
def test_owned_metadata_matches_the_display_format(
    depth: int,
    byte_order: str,
    transform: bool,
    readback: bool,
    tmp_path: Path,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    config = component_config_path("owned.yaml").read_text()
    config = config.replace("color_depth: 16", f"color_depth: {depth}")
    config = config.replace("byte_order: little_endian", f"byte_order: {byte_order}")
    path = tmp_path / "format.yaml"
    path.write_text(config)
    main = generate_main(path)
    metadata = CORE.data.get("mipi_dsi", {})
    assert (
        any(str(value) == "panel" for value in metadata.get("rgb565_transform_ids", []))
        is transform
    )
    assert (
        any(
            str(value) == "panel"
            for value in metadata.get("framebuffer_readback_ids", [])
        )
        is readback
    )
    assert "lvgl::ROTATION_SOFTWARE" in main
    assert (
        "USE_MIPI_DSI_RGB565_OWNED_TRANSFORM" in {d.name for d in CORE.defines}
    ) is transform
    assert "mipi_dsi::MipiDsi" in main


def test_lvgl_rotation_with_another_display(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main = generate_main(component_config_path("lvgl-mixed.yaml"))
    assert "lvgl::ROTATION_SOFTWARE" in main
