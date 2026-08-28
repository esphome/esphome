"""Tests for display component metadata functions."""

from unittest.mock import patch

import pytest

from esphome.components.const import BYTE_ORDER_BIG, BYTE_ORDER_LITTLE
from esphome.components.display import (
    DisplayMetaData,
    add_metadata,
    get_all_display_metadata,
    get_display_metadata,
)
from esphome.config import Config
from esphome.core import ID
from esphome.final_validate import full_config


def test_add_metadata_basic():
    """Test adding metadata with an ID object."""
    with patch("esphome.components.display.CORE.data", {}):
        add_metadata(ID("my_display"), 320, 240)
        meta = get_display_metadata(ID("my_display"))
        assert meta == DisplayMetaData(
            width=320,
            height=240,
            has_hardware_rotation=False,
            byte_order=BYTE_ORDER_BIG,
        )


def test_add_metadata_with_all_fields():
    """Test adding metadata with all fields set."""
    with patch("esphome.components.display.CORE.data", {}):
        add_metadata(
            ID("my_display"),
            480,
            320,
            has_hardware_rotation=True,
            byte_order=BYTE_ORDER_LITTLE,
        )
        meta = get_display_metadata(ID("my_display"))
        assert meta == DisplayMetaData(
            width=480,
            height=320,
            has_hardware_rotation=True,
            byte_order=BYTE_ORDER_LITTLE,
        )


def test_add_metadata_hardware_rotation_default():
    """Test that has_hardware_rotation defaults to False."""
    with patch("esphome.components.display.CORE.data", {}):
        add_metadata(ID("disp"), 128, 64)
        meta = get_display_metadata(ID("disp"))
        assert meta.has_hardware_rotation is False
        assert meta.byte_order == BYTE_ORDER_BIG


def test_add_metadata_with_byte_order():
    """Test adding metadata with explicit byte_order."""
    with patch("esphome.components.display.CORE.data", {}):
        add_metadata(ID("disp"), 240, 320, byte_order=BYTE_ORDER_LITTLE)
        meta = get_display_metadata(ID("disp"))
        assert meta.byte_order == BYTE_ORDER_LITTLE


def test_get_display_metadata_missing_reads_raw_config():
    """Querying a non-existent ID falls back to raw config lookup."""
    with patch("esphome.components.display.CORE.data", {}):
        # Set up a minimal full_config with a display entry so the fallback
        # path in get_display_metadata can find the display config.
        fc = Config()
        fc["display"] = [
            {
                "id": ID("no_such_display", True),
                "auto_clear_enabled": True,
                "dimensions": {"width": 320, "height": 240},
                "byte_order": BYTE_ORDER_LITTLE,
                "rotation": 90,
            },
            {
                "id": ID("other_display", True),
                "auto_clear_enabled": "undefined",
                "dimensions": (1024, 600),
            },
        ]
        fc.declare_ids.append((ID("no_such_display", True), ["display", 0, "id"]))
        fc.declare_ids.append((ID("other_display", True), ["display", 1, "id"]))
        full_config.set(fc)
        data = get_display_metadata(ID("no_such_display"))
        assert data.width == 320
        assert data.height == 240
        assert data.has_hardware_rotation is False
        assert data.has_writer is True
        assert data.byte_order == BYTE_ORDER_LITTLE
        assert data.rotation == 90

        data = get_display_metadata(ID("other_display"))
        assert data.width == 1024
        assert data.height == 600
        assert data.has_writer is False


def test_add_multiple_displays():
    """Test adding metadata for multiple displays."""
    with patch("esphome.components.display.CORE.data", {}):
        add_metadata(ID("disp_a"), 320, 240)
        add_metadata(ID("disp_b"), 128, 64, has_hardware_rotation=True)

        all_meta = get_all_display_metadata()
        assert len(all_meta) == 2
        assert all_meta["disp_a"] == DisplayMetaData(320, 240, False)
        assert all_meta["disp_b"] == DisplayMetaData(128, 64, True, BYTE_ORDER_BIG)


def test_add_duplicate_id_asserts():
    """Adding metadata for the same ID object twice should assert."""
    with patch("esphome.components.display.CORE.data", {}):
        id_obj = ID("disp")
        add_metadata(id_obj, 320, 240)
        with pytest.raises(AssertionError, match="Duplicate"):
            add_metadata(id_obj, 640, 480)


def test_metadata_is_frozen():
    """Test that DisplayMetaData instances are immutable (frozen dataclass)."""
    meta = DisplayMetaData(320, 240, False, BYTE_ORDER_BIG)
    with pytest.raises(AttributeError):
        meta.width = 640
    with pytest.raises(AttributeError):
        meta.byte_order = BYTE_ORDER_LITTLE


def test_get_all_metadata_asserts_on_unresolved_id():
    """get_all_display_metadata should assert if any ID has id=None."""
    with patch("esphome.components.display.CORE.data", {}):
        add_metadata(ID(None), 320, 240)
        with pytest.raises(AssertionError, match="resolved"):
            get_all_display_metadata()


def test_get_metadata_asserts_on_unresolved_id():
    """get_display_metadata should assert if any ID has id=None."""
    with patch("esphome.components.display.CORE.data", {}):
        add_metadata(ID(None), 320, 240)
        with pytest.raises(AssertionError, match="resolved"):
            get_display_metadata(ID("anything"))
