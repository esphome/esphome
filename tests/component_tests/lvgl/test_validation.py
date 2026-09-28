"""Tests for LVGL final_validation display metadata checks."""

from __future__ import annotations

import pytest

from esphome.components.const import BYTE_ORDER_BIG, BYTE_ORDER_LITTLE, CONF_BYTE_ORDER
from esphome.components.display import add_metadata
from esphome.components.lvgl import final_validation
from esphome.config import Config
from esphome.config_validation import Invalid
from esphome.const import KEY_CORE, KEY_TARGET_FRAMEWORK, KEY_TARGET_PLATFORM
from esphome.core import CORE, ID
from esphome.final_validate import full_config


@pytest.fixture(autouse=True)
def _setup_core():
    """Ensure CORE.data has enough context for final_validation."""
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: "host",
        KEY_TARGET_FRAMEWORK: "",
    }
    full_config.set(Config())
    yield
    CORE.reset()


def _register_displays(*display_ids: str) -> None:
    """Register display IDs in full_config so get_path_for_id works."""
    fc = full_config.get()
    display_list = [{"id": ID(d, True)} for d in display_ids]
    fc["display"] = display_list
    for i, disp_id in enumerate(display_ids):
        fc.declare_ids.append((ID(disp_id, True), ["display", i, "id"]))


def _make_lvgl_config(
    display_ids: list[str],
    byte_order: str | None = None,
) -> dict:
    """Build a minimal LVGL config dict for final_validation."""
    _register_displays(*display_ids)
    config = {
        "displays": [ID(d, True) for d in display_ids],
        "log_level": "WARN",
        "color_depth": 16,
        "transparency_key": 0x000400,
        "draw_rounding": 2,
        "buffer_size": 0,
    }
    if byte_order is not None:
        config[CONF_BYTE_ORDER] = byte_order
    return config


class TestByteOrderAutoConfig:
    """Test that LVGL auto-configures byte_order from display metadata."""

    def test_inherits_big_endian_from_display(self) -> None:
        """LVGL should inherit big_endian from display metadata."""
        add_metadata(ID("my_disp"), 320, 240, byte_order=BYTE_ORDER_BIG)
        configs = [_make_lvgl_config(["my_disp"])]
        final_validation(configs)
        assert configs[0][CONF_BYTE_ORDER] == BYTE_ORDER_BIG

    def test_inherits_little_endian_from_display(self) -> None:
        """LVGL should inherit little_endian from display metadata."""
        add_metadata(ID("my_disp"), 320, 240, byte_order=BYTE_ORDER_LITTLE)
        configs = [_make_lvgl_config(["my_disp"])]
        final_validation(configs)
        assert configs[0][CONF_BYTE_ORDER] == BYTE_ORDER_LITTLE

    def test_defaults_to_big_endian_when_no_metadata(self) -> None:
        """LVGL should default to big_endian when display has no metadata."""
        configs = [_make_lvgl_config(["my_disp"])]
        final_validation(configs)
        assert configs[0][CONF_BYTE_ORDER] == BYTE_ORDER_BIG


class TestByteOrderExplicitMismatchError:
    """Test that LVGL rejects explicit byte_order mismatch with display."""

    def test_raises_on_mismatch(self) -> None:
        """Explicit LVGL byte_order different from display should raise."""
        add_metadata(ID("my_disp"), 320, 240, byte_order=BYTE_ORDER_LITTLE)
        configs = [_make_lvgl_config(["my_disp"], byte_order=BYTE_ORDER_BIG)]
        with pytest.raises(
            Invalid, match="LVGL byte order must match the display byte order"
        ):
            final_validation(configs)

    def test_no_error_when_matching(self) -> None:
        """Explicit LVGL byte_order matching display should pass."""
        add_metadata(ID("my_disp"), 320, 240, byte_order=BYTE_ORDER_BIG)
        configs = [_make_lvgl_config(["my_disp"], byte_order=BYTE_ORDER_BIG)]
        final_validation(configs)


class TestByteOrderMultipleDisplays:
    """Test byte_order validation with multiple displays."""

    def test_consistent_displays_inherit(self) -> None:
        """All displays with same byte_order should set LVGL byte_order."""
        add_metadata(ID("disp_a"), 320, 240, byte_order=BYTE_ORDER_LITTLE)
        add_metadata(ID("disp_b"), 128, 64, byte_order=BYTE_ORDER_LITTLE)
        configs = [_make_lvgl_config(["disp_a", "disp_b"])]
        final_validation(configs)
        assert configs[0][CONF_BYTE_ORDER] == BYTE_ORDER_LITTLE

    def test_inconsistent_displays_raises(self) -> None:
        """Displays with different byte_order should raise an error."""
        add_metadata(ID("disp_a"), 320, 240, byte_order=BYTE_ORDER_BIG)
        add_metadata(ID("disp_b"), 128, 64, byte_order=BYTE_ORDER_LITTLE)
        configs = [_make_lvgl_config(["disp_a", "disp_b"])]
        with pytest.raises(Invalid, match="same byte_order"):
            final_validation(configs)


class TestHasWriterCheck:
    """Test that LVGL rejects displays with has_writer set."""

    def test_display_with_writer_raises(self) -> None:
        """Display with lambda/pages/auto_clear should be rejected."""
        add_metadata(ID("my_disp"), 320, 240, has_writer=True)
        configs = [_make_lvgl_config(["my_disp"])]
        with pytest.raises(Invalid, match="not compatible with LVGL"):
            final_validation(configs)

    def test_display_without_writer_passes(self) -> None:
        """Display without writer should pass."""
        add_metadata(ID("my_disp"), 320, 240, has_writer=False)
        configs = [_make_lvgl_config(["my_disp"])]
        final_validation(configs)


class TestRotationCheck:
    """Test that LVGL rejects displays with non-zero rotation."""

    def test_display_with_rotation_raises(self) -> None:
        """Display with rotation should be rejected."""
        add_metadata(ID("my_disp"), 320, 240, rotation=90)
        configs = [_make_lvgl_config(["my_disp"])]
        with pytest.raises(Invalid, match="rotation.*not compatible with LVGL"):
            final_validation(configs)

    def test_display_without_rotation_passes(self) -> None:
        """Display with rotation=0 should pass."""
        add_metadata(ID("my_disp"), 320, 240, rotation=0)
        configs = [_make_lvgl_config(["my_disp"])]
        final_validation(configs)


class TestDrawRoundingMerge:
    """Test that display draw_rounding is merged into LVGL config."""

    def test_display_draw_rounding_overrides_lower(self) -> None:
        """Display draw_rounding higher than LVGL default should win."""
        add_metadata(ID("my_disp"), 320, 240, draw_rounding=8)
        configs = [_make_lvgl_config(["my_disp"])]
        final_validation(configs)
        assert configs[0]["draw_rounding"] == 8

    def test_display_draw_rounding_does_not_lower(self) -> None:
        """Display draw_rounding lower than LVGL config should not reduce it."""
        add_metadata(ID("my_disp"), 320, 240, draw_rounding=1)
        configs = [_make_lvgl_config(["my_disp"])]
        configs[0]["draw_rounding"] = 4
        final_validation(configs)
        assert configs[0]["draw_rounding"] == 4

    def test_zero_draw_rounding_no_change(self) -> None:
        """Display with draw_rounding=0 should not affect LVGL config."""
        add_metadata(ID("my_disp"), 320, 240, draw_rounding=0)
        configs = [_make_lvgl_config(["my_disp"])]
        final_validation(configs)
        assert configs[0]["draw_rounding"] == 2
