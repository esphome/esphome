"""Tests for the lazy build behaviour of `lvgl.<widget>.update` action schemas.

`WidgetType.__init__` registers each widget's update-action schema lazily so
that ~200 ms of cumulative voluptuous work is skipped at lvgl import time
for YAMLs that never invoke an update action. The registry stores
`raw_schema` directly, so the build must be eager when the language-schema
dumper is collecting structure; these tests pin both halves.
"""

from __future__ import annotations

from unittest.mock import patch

from esphome.automation import ACTION_REGISTRY

# Importing the lvgl package populates the WIDGET_TYPES and ACTION_REGISTRY
# entries used by these tests as a side effect.
import esphome.components.lvgl  # noqa: F401
from esphome.components.lvgl.schemas import WIDGET_TYPES
from esphome.components.lvgl.widgets import _update_action_schema
from esphome.config_validation import Schema


def _widget_type(name: str = "obj"):
    wt = WIDGET_TYPES.get(name)
    assert wt is not None, f"widget type {name!r} not registered"
    return wt


def test_registry_entry_uses_lazy_validator() -> None:
    """The default import path (EnableSchemaExtraction off) must store a
    bare callable in `raw_schema`, not a fully materialised `Schema`. A
    `Schema` instance here means the schema was built eagerly at import."""
    entry = ACTION_REGISTRY["lvgl.label.update"]
    assert callable(entry.raw_schema), "raw_schema must be callable"
    assert not isinstance(entry.raw_schema, Schema), (
        "raw_schema should be the lazy validator, not a materialised Schema; "
        "an eagerly built Schema means the import-time deferral was reverted"
    )


def test_lazy_validator_defers_build_until_first_call() -> None:
    """`_update_action_schema()` must not invoke `_build_update_schema` at
    construction time; only the first call to the returned validator should
    trigger the build."""
    wt = _widget_type("label")
    with patch(
        "esphome.components.lvgl.widgets._build_update_schema",
        wraps=lambda w: Schema({}),
    ) as build_mock:
        validator = _update_action_schema(wt)
        assert build_mock.call_count == 0, (
            "_build_update_schema must not be invoked at construction time"
        )
        validator({})
        assert build_mock.call_count == 1, (
            "_build_update_schema must be invoked exactly once on first call"
        )
        validator({})
        assert build_mock.call_count == 1, (
            "_build_update_schema must not be invoked again on subsequent calls"
        )


def test_eager_build_when_schema_extraction_enabled() -> None:
    """When `EnableSchemaExtraction` is on (set by build_language_schema.py
    before importing esphome modules), `_update_action_schema()` must
    materialise the schema eagerly so the dumper can introspect the mapping;
    a bare validator would render as opaque in the language schema output."""
    wt = _widget_type("label")
    with patch("esphome.components.lvgl.widgets.EnableSchemaExtraction", True):
        result = _update_action_schema(wt)
    assert isinstance(result, Schema), (
        "with EnableSchemaExtraction=True, _update_action_schema must return "
        "a Schema instance so schema_extractors can walk its mapping"
    )


def test_lazy_and_eager_produce_equivalent_validation() -> None:
    """The lazy validator and an eagerly built schema must accept the same
    inputs; a bug in the deferral or the cached materialisation could yield
    a divergent validator while still being structurally callable."""
    wt = _widget_type("label")
    with patch("esphome.components.lvgl.widgets.EnableSchemaExtraction", True):
        eager = _update_action_schema(wt)
    lazy = _update_action_schema(wt)

    sample = {"id": "label_id"}
    assert lazy(sample) == eager(sample)
