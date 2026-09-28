"""Tests for voluptuous_schema.py."""

import pytest
import voluptuous as vol

from esphome.voluptuous_schema import _Schema


class TestIdKeyDropping:
    """Test that 'id' keys are silently dropped in PREVENT_EXTRA schemas."""

    def test_id_key_silently_dropped(self):
        """Schema without 'id' should accept and drop 'id' key from input."""
        schema = _Schema(
            {
                vol.Required("name"): str,
                vol.Optional("value", default=0): int,
            }
        )
        result = schema({"name": "test", "value": 42, "id": "my_id"})
        assert result == {"name": "test", "value": 42}
        assert "id" not in result

    def test_id_key_dropped_with_only_required(self):
        """Schema with only required keys should still drop 'id'."""
        schema = _Schema(
            {
                vol.Required("source"): str,
            }
        )
        result = schema({"source": "github://test", "id": "my_component"})
        assert result == {"source": "github://test"}

    def test_other_extra_keys_still_rejected(self):
        """Non-'id' extra keys should still raise errors."""
        schema = _Schema(
            {
                vol.Required("name"): str,
            }
        )
        with pytest.raises(vol.MultipleInvalid, match="extra keys not allowed"):
            schema({"name": "test", "unknown_key": "value"})

    def test_id_key_not_dropped_when_in_schema(self):
        """When 'id' is declared in the schema, it should be validated normally."""
        schema = _Schema(
            {
                vol.Required("id"): str,
                vol.Required("name"): str,
            }
        )
        result = schema({"id": "my_id", "name": "test"})
        assert result == {"id": "my_id", "name": "test"}

    def test_id_key_not_dropped_with_allow_extra(self):
        """With ALLOW_EXTRA, 'id' should be kept (not dropped)."""
        schema = _Schema(
            {
                vol.Required("name"): str,
            },
            extra=vol.ALLOW_EXTRA,
        )
        result = schema({"name": "test", "id": "my_id"})
        assert result == {"name": "test", "id": "my_id"}

    def test_id_key_dropped_with_remove_extra(self):
        """With REMOVE_EXTRA, 'id' should be removed along with other extras."""
        schema = _Schema(
            {
                vol.Required("name"): str,
            },
            extra=vol.REMOVE_EXTRA,
        )
        result = schema({"name": "test", "id": "my_id", "other": "value"})
        assert result == {"name": "test"}

    def test_without_id_no_extra_keys(self):
        """Normal validation without 'id' key should work as before."""
        schema = _Schema(
            {
                vol.Required("name"): str,
                vol.Optional("value", default=0): int,
            }
        )
        result = schema({"name": "test"})
        assert result == {"name": "test", "value": 0}
