"""Tests for display component metadata functions."""

from unittest.mock import patch

from esphome.components.display import (
    DisplayMetaData,
    add_metadata,
    get_all_display_metadata,
    get_display_metadata,
)
from esphome.cpp_generator import MockObj


def test_add_metadata_with_string_id():
    """Test adding metadata with a plain string ID."""
    with patch("esphome.components.display.CORE.data", {}):
        add_metadata("my_display", 320, 240, True)
        meta = get_display_metadata("my_display")
        assert meta == DisplayMetaData(
            width=320, height=240, has_writer=True, has_hardware_rotation=False
        )


def test_add_metadata_with_mockobj_id():
    """Test adding metadata with a MockObj ID (converted via str())."""
    with patch("esphome.components.display.CORE.data", {}):
        mock_id = MockObj("my_display_obj")
        add_metadata(mock_id, 480, 320, False, has_hardware_rotation=True)
        meta = get_display_metadata("my_display_obj")
        assert meta == DisplayMetaData(
            width=480, height=320, has_writer=False, has_hardware_rotation=True
        )


def test_add_metadata_hardware_rotation_default():
    """Test that has_hardware_rotation defaults to False."""
    with patch("esphome.components.display.CORE.data", {}):
        add_metadata("disp", 128, 64, False)
        meta = get_display_metadata("disp")
        assert meta.has_hardware_rotation is False


def test_get_display_metadata_missing_returns_none():
    """Test that querying a non-existent ID returns None."""
    with patch("esphome.components.display.CORE.data", {}):
        data = get_display_metadata("no_such_display")
        assert data.width == 0
        assert data.height == 0
        assert data.has_writer is False
        assert data.has_hardware_rotation is False


def test_add_multiple_displays():
    """Test adding metadata for multiple displays."""
    with patch("esphome.components.display.CORE.data", {}):
        add_metadata("disp_a", 320, 240, True)
        add_metadata("disp_b", 128, 64, False, has_hardware_rotation=True)

        all_meta = get_all_display_metadata()
        assert len(all_meta) == 2
        assert all_meta["disp_a"] == DisplayMetaData(320, 240, True, False)
        assert all_meta["disp_b"] == DisplayMetaData(128, 64, False, True)


def test_add_metadata_overwrites_existing():
    """Test that adding metadata for the same ID overwrites the previous entry."""
    with patch("esphome.components.display.CORE.data", {}):
        add_metadata("disp", 320, 240, True)
        add_metadata("disp", 640, 480, False, has_hardware_rotation=True)
        meta = get_display_metadata("disp")
        assert meta == DisplayMetaData(640, 480, False, True)


def test_metadata_is_frozen():
    """Test that DisplayMetaData instances are immutable (frozen dataclass)."""
    meta = DisplayMetaData(320, 240, True, False)
    try:
        meta.width = 640
        assert False, "Expected FrozenInstanceError"
    except AttributeError:
        pass
