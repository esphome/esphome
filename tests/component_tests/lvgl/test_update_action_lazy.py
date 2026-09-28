"""Tests for lvgl.<widget>.update lazy schema build."""

from __future__ import annotations

from unittest.mock import patch

from esphome.automation import ACTION_REGISTRY
import esphome.components.lvgl  # noqa: F401
from esphome.components.lvgl.schemas import WIDGET_TYPES
from esphome.components.lvgl.widgets import _update_action_schema
from esphome.config_validation import Schema


def _widget_type(name: str = "obj"):
    wt = WIDGET_TYPES.get(name)
    assert wt is not None, f"widget type {name!r} not registered"
    return wt


def test_registry_entry_uses_lazy_validator() -> None:
    entry = ACTION_REGISTRY["lvgl.label.update"]
    assert callable(entry.raw_schema)
    assert not isinstance(entry.raw_schema, Schema)


def test_lazy_validator_defers_build_until_first_call() -> None:
    wt = _widget_type("label")
    with patch(
        "esphome.components.lvgl.widgets._build_update_schema",
        wraps=lambda w: Schema({}),
    ) as build_mock:
        validator = _update_action_schema(wt)
        assert build_mock.call_count == 0
        validator({})
        assert build_mock.call_count == 1
        validator({})
        assert build_mock.call_count == 1


def test_eager_build_when_schema_extraction_enabled() -> None:
    wt = _widget_type("label")
    with patch("esphome.components.lvgl.widgets.EnableSchemaExtraction", True):
        result = _update_action_schema(wt)
    assert isinstance(result, Schema)


def test_lazy_and_eager_produce_equivalent_validation() -> None:
    wt = _widget_type("label")
    with patch("esphome.components.lvgl.widgets.EnableSchemaExtraction", True):
        eager = _update_action_schema(wt)
    lazy = _update_action_schema(wt)
    sample = {"id": "label_id"}
    assert lazy(sample) == eager(sample)
