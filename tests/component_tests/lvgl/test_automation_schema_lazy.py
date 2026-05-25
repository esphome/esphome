"""Tests for lvgl automation_schema lazy validate_automation build."""

from __future__ import annotations

from unittest.mock import patch

import esphome.components.lvgl  # noqa: F401
from esphome.components.lvgl import schemas as lvgl_schemas
from esphome.components.lvgl.schemas import (
    WIDGET_TYPES,
    _lazy_validate_automation,
    automation_schema,
)
from esphome.components.lvgl.widgets import WidgetType
from esphome.config_validation import GenerateID, declare_id
from esphome.const import CONF_TRIGGER_ID
from esphome.core.config import StartupTrigger


def _widget_type(name: str = "obj") -> WidgetType:
    wt = WIDGET_TYPES.get(name)
    assert wt is not None, f"widget type {name!r} not registered"
    return wt


def _trigger_extra_schema() -> dict:
    return {GenerateID(CONF_TRIGGER_ID): declare_id(StartupTrigger)}


def test_lazy_validator_defers_build_until_first_call() -> None:
    with patch(
        "esphome.components.lvgl.schemas.validate_automation",
        wraps=lvgl_schemas.validate_automation,
    ) as va_mock:
        validator = _lazy_validate_automation(_trigger_extra_schema())
        assert va_mock.call_count == 0
        validator({"then": []})
        assert va_mock.call_count == 1
        validator({"then": []})
        assert va_mock.call_count == 1


def test_eager_build_when_schema_extraction_enabled() -> None:
    with (
        patch("esphome.components.lvgl.schemas.EnableSchemaExtraction", True),
        patch(
            "esphome.components.lvgl.schemas.validate_automation",
            wraps=lvgl_schemas.validate_automation,
        ) as va_mock,
    ):
        _lazy_validate_automation(_trigger_extra_schema())
        assert va_mock.call_count == 1


def test_lazy_and_eager_produce_equivalent_validation() -> None:
    extra = _trigger_extra_schema()
    with patch("esphome.components.lvgl.schemas.EnableSchemaExtraction", True):
        eager = _lazy_validate_automation(extra)
    lazy = _lazy_validate_automation(_trigger_extra_schema())
    sample = {"then": []}
    assert lazy(sample) == eager(sample)


def test_automation_schema_uses_lazy_validators() -> None:
    wt = _widget_type("obj")
    with patch(
        "esphome.components.lvgl.schemas.validate_automation",
        wraps=lvgl_schemas.validate_automation,
    ) as va_mock:
        automation_schema(wt.w_type)
        assert va_mock.call_count == 0
