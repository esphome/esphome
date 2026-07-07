"""Tests for part_dict / obj_dict / part_schema / obj_schema mapping contracts.

These guard the dict-merge refactor: the dict helpers must keep returning the
same logical mapping as the chained-extend version produced, and the
corresponding Schema(...) wrappers must accept and reject the same configs.
"""

from __future__ import annotations

from collections.abc import Generator

import pytest
import voluptuous as vol

from esphome import config_validation as cv
import esphome.components.lvgl
from esphome.components.lvgl import (
    _theme_schema,
    defines as df,
    schemas as lvgl_schemas,
)
from esphome.components.lvgl.schemas import (
    ALIGN_TO_SCHEMA,
    FLAG_SCHEMA,
    FULL_STYLE_SCHEMA,
    STATE_SCHEMA,
    STYLE_SCHEMA,
    WIDGET_TYPES,
    automation_schema,
    obj_dict,
    obj_schema,
    part_dict,
    part_schema,
)
from esphome.components.lvgl.types import LvType
from esphome.components.lvgl.widgets import WidgetType


@pytest.fixture(autouse=True)
def _clear_obj_dict_cache() -> Generator[None]:
    cache = getattr(lvgl_schemas, "_OBJ_DICT_CACHE", None)
    if cache is not None:
        cache.clear()
    # The lazily-built theme schema is cached on _build_theme_schema; clear it
    # too so each test starts from a clean slate.
    build_theme = getattr(esphome.components.lvgl, "_build_theme_schema", None)
    if build_theme is not None and hasattr(build_theme, "cache_clear"):
        build_theme.cache_clear()
    yield
    if cache is not None:
        cache.clear()
    if build_theme is not None and hasattr(build_theme, "cache_clear"):
        build_theme.cache_clear()


def _marker_names(mapping) -> set[str]:
    """Return the underlying string names of every voluptuous Marker key."""
    names: set[str] = set()
    for key in mapping:
        if isinstance(key, vol.Marker):
            schema = key.schema
            if isinstance(schema, str):
                names.add(schema)
    return names


def _widget_type(name: str = "obj"):
    wt = WIDGET_TYPES.get(name)
    assert wt is not None, f"widget type {name!r} not registered"
    return wt


def test_part_dict_includes_state_flag_and_part_keys() -> None:
    parts = ("indicator", "knob")
    keys = _marker_names(part_dict(parts))

    assert {"indicator", "knob"} <= keys
    assert _marker_names(STATE_SCHEMA.schema) <= keys
    assert _marker_names(FLAG_SCHEMA.schema) <= keys


def test_obj_dict_extends_part_dict_with_align_automation_state_group() -> None:
    wt = _widget_type("obj")
    part_keys = _marker_names(part_dict(wt.parts))
    obj_keys = _marker_names(obj_dict(wt))

    assert part_keys <= obj_keys
    assert _marker_names(ALIGN_TO_SCHEMA) <= obj_keys
    assert _marker_names(automation_schema(wt.w_type)) <= obj_keys
    assert {"state", "group"} <= obj_keys


def test_obj_dict_is_memoized_by_widget_type() -> None:
    wt = _widget_type("obj")
    first = obj_dict(wt)
    second = obj_dict(wt)
    assert first is second
    # Different widget type, different dict.
    assert obj_dict(_widget_type("label")) is not first


def test_part_schema_round_trips_known_state_and_part_settings() -> None:
    schema = part_schema(("indicator",))
    out = schema(
        {
            "bg_color": 0x112233,
            "checked": {"bg_color": 0x445566},
            "indicator": {"bg_color": 0x778899},
        }
    )
    assert out["bg_color"] == 0x112233
    assert out["checked"]["bg_color"] == 0x445566
    assert out["indicator"]["bg_color"] == 0x778899


def test_part_schema_rejects_unknown_part() -> None:
    schema = part_schema(("indicator",))
    with pytest.raises(vol.Invalid):
        schema({"definitely_not_a_part": {}})


@pytest.mark.parametrize("name", sorted(WIDGET_TYPES))
def test_obj_schema_accepts_empty_config_for_every_widget_type(name: str) -> None:
    obj_schema(_widget_type(name))({})


def test_obj_schema_accepts_align_to_and_state_group() -> None:
    schema = obj_schema(_widget_type("obj"))
    out = schema(
        {
            df.CONF_ALIGN_TO: {
                "id": "some_other_widget",
                df.CONF_ALIGN: "TOP_LEFT",
            },
            "state": {"checked": True},
        }
    )
    assert out[df.CONF_ALIGN_TO][df.CONF_ALIGN] == "LV_ALIGN_TOP_LEFT"
    assert out["state"]["checked"] is True


def test_obj_schema_rejects_unknown_top_level_key() -> None:
    with pytest.raises(vol.Invalid):
        obj_schema(_widget_type("obj"))({"definitely_not_a_real_key": 1})


def test_part_schema_returns_cv_schema_for_extend_callers() -> None:
    schema = part_schema(("indicator",))
    extended = schema.extend({cv.Optional("extra_key"): cv.string})
    out = extended({"extra_key": "value", "bg_color": 0xAABBCC})
    assert out["extra_key"] == "value"
    assert out["bg_color"] == 0xAABBCC


def test_obj_schema_returns_cv_schema_for_extend_callers() -> None:
    schema = obj_schema(_widget_type("obj"))
    extended = schema.extend({cv.Optional("extra_key"): cv.string})
    extended({"extra_key": "value"})


@pytest.mark.parametrize(
    "schema",
    [STATE_SCHEMA, FLAG_SCHEMA, STYLE_SCHEMA, FULL_STYLE_SCHEMA],
)
def test_spread_sources_carry_no_extra_schemas(schema: cv.Schema) -> None:
    # part_dict / obj_dict reach into .schema and rebuild via cv.Schema(...),
    # which silently drops _extra_schemas and any non-default extra/required.
    # Lock the invariant so a future add_extra() on these sources fails CI
    # instead of quietly removing validation from part/obj/theme schemas.
    assert not schema._extra_schemas
    assert schema.extra is vol.PREVENT_EXTRA
    assert schema.required is False


def test_theme_schema_merges_obj_dict_and_full_style_props() -> None:
    # _theme_schema is the riskiest merge: obj_dict(w) and FULL_STYLE_SCHEMA.schema
    # share many STYLE_SCHEMA marker instances. Exercise the merged schema
    # end-to-end with one key from each side (a STATE_SCHEMA part from obj_dict
    # and a FULL_STYLE-only property) to lock the behaviour against future
    # regressions in either source.
    out = _theme_schema(
        {
            df.CONF_DARK_MODE: True,
            "obj": {
                "bg_color": 0x112233,
                "checked": {"bg_color": 0x445566},
                df.CONF_PAD_ROW: 4,
                df.CONF_GRID_CELL_X_ALIGN: "CENTER",
            },
        }
    )
    assert out[df.CONF_DARK_MODE] is True
    obj_out = out["obj"]
    assert obj_out["bg_color"] == 0x112233
    assert obj_out["checked"]["bg_color"] == 0x445566
    assert obj_out[df.CONF_PAD_ROW] == 4
    assert obj_out[df.CONF_GRID_CELL_X_ALIGN] == "LV_GRID_ALIGN_CENTER"


def test_theme_schema_self_heals_when_a_widget_type_is_registered_later() -> None:
    # _build_theme_schema is functools.cached on a snapshot of WIDGET_TYPES.
    # any_widget_schema explicitly supports external components registering
    # widgets lazily, and the device builder revalidates in-process, so a
    # widget registered after first use must invalidate the cached snapshot.
    _theme_schema({df.CONF_DARK_MODE: True})  # populate the cache

    name = "test_self_heal_widget"
    assert name not in WIDGET_TYPES
    # is_mock=True skips registration side-effects; insert into WIDGET_TYPES
    # manually so the next theme call sees the new entry.
    WIDGET_TYPES[name] = WidgetType(name, LvType("test_fake_t"), (), is_mock=True)
    try:
        out = _theme_schema({df.CONF_DARK_MODE: False, name: {"bg_color": 0x010203}})
        assert out[name]["bg_color"] == 0x010203
    finally:
        WIDGET_TYPES.pop(name, None)


@pytest.mark.parametrize(
    "schema",
    [STATE_SCHEMA, FLAG_SCHEMA, STYLE_SCHEMA, FULL_STYLE_SCHEMA],
)
def test_spread_sources_have_no_top_level_marker_defaults(schema: cv.Schema) -> None:
    # _theme_schema merges obj_dict(w) with FULL_STYLE_SCHEMA.schema; on a key
    # collision, dict-spread keeps the first source's marker (and its default)
    # but the last source's value, whereas .extend() would take both from the
    # later source. The two are equivalent today because the overlapping
    # markers are the same instances (both derive from STYLE_SCHEMA) and none
    # carry a top-level default. Lock that so a future divergent default would
    # fail CI rather than silently drift the merged validation.
    offenders = [
        marker.schema
        for marker in schema.schema
        if isinstance(marker, vol.Optional) and marker.default is not vol.UNDEFINED
    ]
    assert not offenders, f"top-level Optional with default: {offenders}"
