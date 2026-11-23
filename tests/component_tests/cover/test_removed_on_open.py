"""Tests for the cover on_open trigger removed in 2026.12.0."""

import pytest
from voluptuous import Invalid

from esphome.components.cover import Cover, cover_schema


def test_on_open_names_on_opened() -> None:
    with pytest.raises(Invalid, match="use 'on_opened' instead"):
        cover_schema(Cover)({"name": "Blind", "on_open": []})
