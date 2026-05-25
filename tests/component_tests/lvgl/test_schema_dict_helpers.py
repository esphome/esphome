"""Tests for part_dict / obj_dict / part_schema / obj_schema mapping contracts.

These guard the dict-merge refactor: the dict helpers must keep returning the
same logical mapping as the chained-extend version produced, and the
corresponding Schema(...) wrappers must accept and reject the same configs.
"""

from __future__ import annotations

import pytest
import voluptuous as vol

from esphome import config_validation as cv
import esphome.components.lvgl  # noqa: F401
from esphome.components.lvgl import defines as df
from esphome.components.lvgl.schemas import (
    ALIGN_TO_SCHEMA,
    FLAG_SCHEMA,
    STATE_SCHEMA,
    WIDGET_TYPES,
    automation_schema,
    obj_dict,
    obj_schema,
    part_dict,
    part_schema,
)


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
