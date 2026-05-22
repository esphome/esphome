"""Tests for the memoization / lazy build behaviour of `container_schema()`.

The validator returned by ``container_schema()`` is cached keyed by
``(id(widget_type), id(extras))`` and the underlying voluptuous schema is
materialised only on first validation. These tests pin those invariants;
without them a refactor could silently revert the per-validation speedup
without breaking the existing functional lvgl test configs.
"""

from __future__ import annotations

from collections.abc import Generator
from unittest.mock import patch

import pytest

from esphome import config_validation as cv

# Importing the lvgl package populates the WIDGET_TYPES registry as a side
# effect; we read widget types from it for cache-key differentiation tests.
import esphome.components.lvgl  # noqa: F401
from esphome.components.lvgl import schemas as lvgl_schemas
from esphome.components.lvgl.schemas import WIDGET_TYPES, container_schema


@pytest.fixture(autouse=True)
def _clear_container_schema_cache() -> Generator[None]:
    """The cache is module global; clear it around each test to keep the
    assertions about cache hits / misses independent of import order.

    Tolerant of the cache attribute being absent so the behavioural tests
    still produce meaningful failures (rather than setup errors) if the
    cache layer is ever removed.
    """
    cache = getattr(lvgl_schemas, "_CONTAINER_SCHEMA_CACHE", None)
    if cache is not None:
        cache.clear()
    yield
    if cache is not None:
        cache.clear()


def _widget_type(name: str = "obj"):
    """Return a widget type registered by the lvgl package."""
    wt = WIDGET_TYPES.get(name)
    assert wt is not None, f"widget type {name!r} not registered"
    return wt


def test_same_args_return_same_validator() -> None:
    """Repeat calls with identical arguments must return the cached validator
    instance (the property `any_widget_schema()`'s per-entry loop relies on)."""
    wt = _widget_type("obj")
    first = container_schema(wt)
    second = container_schema(wt)
    assert first is second


def test_extras_none_vs_truthy_get_different_validators() -> None:
    """``extras=None`` and ``extras={...}`` must not collide in the cache;
    the schemas they materialise have different shapes."""
    wt = _widget_type("obj")
    no_extras = container_schema(wt)
    # Use a voluptuous-compatible extras dict so the schema also validates on
    # the dev (pre-memoization) code path; the test is about cache key
    # differentiation, not extras validity.
    extras = {cv.Optional("custom_extra"): cv.string}
    with_extras = container_schema(wt, extras)
    assert no_extras is not with_extras


def test_different_widget_types_get_different_validators() -> None:
    """Different widget types must have distinct cached validators even when
    extras is identical (``None``)."""
    obj = container_schema(_widget_type("obj"))
    label = container_schema(_widget_type("label"))
    assert obj is not label


def test_schema_build_is_deferred_until_first_validation() -> None:
    """``container_schema()`` must not materialise the underlying voluptuous
    schema at construction time; only the first call to the returned
    validator should trigger the build."""
    wt = _widget_type("obj")
    with patch.object(
        lvgl_schemas, "obj_schema", wraps=lvgl_schemas.obj_schema
    ) as obj_schema_mock:
        validator = container_schema(wt)
        assert obj_schema_mock.call_count == 0, (
            "obj_schema() must not be invoked at container_schema() time"
        )
        validator({})
        assert obj_schema_mock.call_count == 1, (
            "obj_schema() must be invoked exactly once on first validation"
        )
        validator({})
        assert obj_schema_mock.call_count == 1, (
            "obj_schema() must not be invoked again on subsequent validations"
        )


def test_cached_validator_produces_equivalent_output() -> None:
    """A validator pulled from the cache must yield the same validated output
    as one produced without the cache for the same input. This protects
    against bugs where the cache returns a stale or wrong validator."""
    wt = _widget_type("obj")
    cached = container_schema(wt)
    # Bypass the cache by clearing it before building a reference.
    cached_result = cached({})
    lvgl_schemas._CONTAINER_SCHEMA_CACHE.clear()
    reference = container_schema(wt)
    assert cached is not reference, "cache should have been cleared"
    assert cached_result == reference({})


def test_id_recycling_is_caught_by_identity_guard() -> None:
    """If a previously cached ``extras`` object is garbage collected and a new
    object reuses its ``id()``, the cache must not return the stale validator.
    Simulated here by mutating the cache to look as if collision happened."""
    wt = _widget_type("obj")
    real_extras = {cv.Optional("a"): cv.int_}
    validator_a = container_schema(wt, real_extras)

    # Find the cache key that was used so we can substitute a fake "old"
    # object that will fail the `is` identity guard when looked up with new
    # extras at the same id().
    cache_key = (id(wt), id(real_extras))
    cached_entry = lvgl_schemas._CONTAINER_SCHEMA_CACHE[cache_key]
    assert cached_entry[1] is real_extras

    # Replace the stored extras with an unrelated object that has the same
    # type but is not identical. Looking up with the original real_extras
    # should now fail the `is` guard and force a rebuild.
    sentinel = {cv.Optional("a"): cv.int_}
    lvgl_schemas._CONTAINER_SCHEMA_CACHE[cache_key] = (
        cached_entry[0],
        sentinel,
        cached_entry[2],
    )

    rebuilt = container_schema(wt, real_extras)
    assert rebuilt is not validator_a, (
        "identity guard must reject the stale entry and rebuild"
    )
