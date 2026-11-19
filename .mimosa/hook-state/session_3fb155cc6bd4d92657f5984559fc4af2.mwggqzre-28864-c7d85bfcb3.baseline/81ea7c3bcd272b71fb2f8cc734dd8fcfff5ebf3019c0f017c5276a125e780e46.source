"""Tests for obj_schema() memoization."""

from __future__ import annotations

from collections.abc import Generator

import pytest

import esphome.components.lvgl  # noqa: F401
from esphome.components.lvgl import schemas as lvgl_schemas
from esphome.components.lvgl.schemas import WIDGET_TYPES, obj_schema


@pytest.fixture(autouse=True)
def _clear_obj_schema_cache() -> Generator[None]:
    cache = getattr(lvgl_schemas, "_OBJ_SCHEMA_CACHE", None)
    if cache is not None:
        cache.clear()
    yield
    if cache is not None:
        cache.clear()


def _widget_type(name: str = "obj"):
    wt = WIDGET_TYPES.get(name)
    assert wt is not None, f"widget type {name!r} not registered"
    return wt


def test_same_widget_type_returns_same_schema() -> None:
    wt = _widget_type("obj")
    assert obj_schema(wt) is obj_schema(wt)


def test_different_widget_types_return_different_schemas() -> None:
    assert obj_schema(_widget_type("obj")) is not obj_schema(_widget_type("label"))


def test_cache_is_populated_after_first_call() -> None:
    wt = _widget_type("obj")
    assert id(wt) not in lvgl_schemas._OBJ_SCHEMA_CACHE
    obj_schema(wt)
    assert id(wt) in lvgl_schemas._OBJ_SCHEMA_CACHE


def test_cached_schema_produces_equivalent_output() -> None:
    wt = _widget_type("obj")
    cached_result = obj_schema(wt)({})
    lvgl_schemas._OBJ_SCHEMA_CACHE.clear()
    fresh_result = obj_schema(wt)({})
    assert cached_result == fresh_result


def test_id_recycling_is_caught_by_identity_guard() -> None:
    wt = _widget_type("obj")
    real_schema = obj_schema(wt)

    cached_widget_type, _ = lvgl_schemas._OBJ_SCHEMA_CACHE[id(wt)]
    sentinel_schema = object()
    lvgl_schemas._OBJ_SCHEMA_CACHE[id(wt)] = (cached_widget_type, sentinel_schema)
    assert obj_schema(wt) is sentinel_schema

    other = _widget_type("label")
    lvgl_schemas._OBJ_SCHEMA_CACHE[id(wt)] = (other, sentinel_schema)
    rebuilt = obj_schema(wt)
    assert rebuilt is not sentinel_schema
    assert rebuilt is not real_schema
