"""Tests for container_schema() memoization and lazy build."""

from __future__ import annotations

from collections.abc import Generator
from unittest.mock import patch

import pytest

from esphome import config_validation as cv
import esphome.components.lvgl  # noqa: F401
from esphome.components.lvgl import schemas as lvgl_schemas
from esphome.components.lvgl.schemas import WIDGET_TYPES, container_schema


@pytest.fixture(autouse=True)
def _clear_container_schema_cache() -> Generator[None]:
    cache = getattr(lvgl_schemas, "_CONTAINER_SCHEMA_CACHE", None)
    if cache is not None:
        cache.clear()
    yield
    if cache is not None:
        cache.clear()


def _widget_type(name: str = "obj"):
    wt = WIDGET_TYPES.get(name)
    assert wt is not None, f"widget type {name!r} not registered"
    return wt


def test_same_args_return_same_validator() -> None:
    wt = _widget_type("obj")
    assert container_schema(wt) is container_schema(wt)


def test_extras_none_vs_truthy_get_different_validators() -> None:
    wt = _widget_type("obj")
    no_extras = container_schema(wt)
    extras = {cv.Optional("custom_extra"): cv.string}
    assert no_extras is not container_schema(wt, extras)


def test_different_widget_types_get_different_validators() -> None:
    assert container_schema(_widget_type("obj")) is not container_schema(
        _widget_type("label")
    )


def test_schema_build_is_deferred_until_first_validation() -> None:
    wt = _widget_type("obj")
    with patch.object(
        lvgl_schemas, "obj_schema", wraps=lvgl_schemas.obj_schema
    ) as obj_schema_mock:
        validator = container_schema(wt)
        assert obj_schema_mock.call_count == 0
        validator({})
        assert obj_schema_mock.call_count == 1
        validator({})
        assert obj_schema_mock.call_count == 1


def test_cached_validator_produces_equivalent_output() -> None:
    wt = _widget_type("obj")
    cached = container_schema(wt)
    cached_result = cached({})
    lvgl_schemas._CONTAINER_SCHEMA_CACHE.clear()
    reference = container_schema(wt)
    assert cached is not reference
    assert cached_result == reference({})


def test_id_recycling_is_caught_by_identity_guard() -> None:
    wt = _widget_type("obj")
    real_extras = {cv.Optional("a"): cv.int_}
    validator_a = container_schema(wt, real_extras)

    cache_key = (id(wt), id(real_extras))
    cached_entry = lvgl_schemas._CONTAINER_SCHEMA_CACHE[cache_key]
    sentinel = {cv.Optional("a"): cv.int_}
    lvgl_schemas._CONTAINER_SCHEMA_CACHE[cache_key] = (
        cached_entry[0],
        sentinel,
        cached_entry[2],
    )

    assert container_schema(wt, real_extras) is not validator_a
