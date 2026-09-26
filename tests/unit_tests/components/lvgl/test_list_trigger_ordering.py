"""Regression test: an lvgl.list.add action must fire a list's on_add trigger
even if it reaches _fire_on_add() before finish_list_triggers() has built that
list's Trigger Pvariable.

ListType.to_code() only records on_add/on_remove configs (via
_get_list_triggers()) - finish_list_triggers() is what actually builds the
Trigger Pvariables from them. An lvgl.list.add action for a list can be
scheduled as part of a different component's own to_code() coroutine, entirely
independent of lvgl's own, so it can reach _fire_on_add() before
finish_list_triggers() has run for that list. _fire_on_add()/_fire_on_remove()
resolve each trigger via cg.get_variable(), which blocks until
finish_list_triggers() builds it - regardless of which of the two jobs the
scheduler happens to run first.

This is reproduced deterministically here (no reliance on incidental component
priority/scheduling) by scheduling the action's job before finish_list_triggers()
on ESPHome's own coroutine scheduler: without cg.get_variable()'s wait, the
action job would run to completion first and observe the trigger as not yet
built.
"""

from __future__ import annotations

from unittest.mock import patch

import pytest

from esphome.automation import ACTION_REGISTRY
from esphome.components.lvgl.lvcode import LvContext
from esphome.components.lvgl.schemas import container_schema
from esphome.components.lvgl.widgets import Widget, widget_to_code
from esphome.components.lvgl.widgets.lv_list import (
    CONF_ON_ADD,
    _get_list_triggers,
    finish_list_triggers,
    list_spec,
)
from esphome.const import CONF_AUTOMATION_ID, CONF_THEN, CONF_TRIGGER_ID, CONF_TYPE_ID
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj, TemplateArguments
from esphome.yaml_util import make_data_base


def _statements() -> list[str]:
    return [str(s) for s in CORE.main_statements]


@pytest.mark.asyncio
async def test_list_add_action_running_before_finish_list_triggers_still_fires_on_add(
    setup_core,
) -> None:
    config = container_schema(list_spec)(
        {
            "id": "test_list",
            CONF_ON_ADD: [{"lambda": make_data_base("return;")}],
        }
    )
    # Auto-generated IDs (trigger/automation/action) are normally resolved to
    # unique names by esphome's full config pass before code generation; do
    # that by hand here since this test only exercises the widget/trigger
    # codegen slice in isolation.
    automation_conf = config[CONF_ON_ADD][0]
    automation_conf[CONF_TRIGGER_ID].resolve([])
    automation_conf[CONF_AUTOMATION_ID].resolve([])
    automation_conf[CONF_THEN][0][CONF_TYPE_ID].resolve([])

    parent = MockObj("parent_obj")
    async with LvContext():
        await widget_to_code(config, list_spec, parent)

    # Schedule the lvgl.list.add action's job before finish_list_triggers()'s -
    # mirroring an action that lives in a different component's automation than
    # lvgl's own to_code(), which can reach this action before lvgl gets to build
    # this list's on_add/on_remove triggers.
    entry = ACTION_REGISTRY["lvgl.list.add"]
    add_config = entry.schema({"id": "test_list", "label": {"text": "row"}})
    action_id = ID("test_list_add_action", is_declaration=True, type=entry.type_id)

    async def run_add_action() -> None:
        async with LvContext():
            await entry.coroutine_fun(add_config, action_id, TemplateArguments(), [])

    CORE.add_job(run_add_action)
    CORE.add_job(finish_list_triggers)
    CORE.flush_tasks()

    statements = _statements()
    assert any("->trigger(" in s for s in statements), (
        "on_add did not fire: the lvgl.list.add action ran before "
        "finish_list_triggers() built the list's on_add trigger, and "
        "_fire_on_add() didn't wait for it"
    )


@pytest.mark.asyncio
async def test_on_add_recorded_before_widget_registered(setup_core) -> None:
    """Widget.create() is what makes a list visible to get_widgets(), so an
    action interleaved with its creation could resolve get_widgets() and reach
    _fire_on_add() right after Widget.create() runs. Its on_add config must
    already be recorded by then - ListType.on_create() (called before
    Widget.create()) is what guarantees that, not ListType.to_code() (called
    after).
    """
    config = container_schema(list_spec)(
        {
            "id": "test_list",
            CONF_ON_ADD: [{"lambda": make_data_base("return;")}],
        }
    )
    automation_conf = config[CONF_ON_ADD][0]
    automation_conf[CONF_TRIGGER_ID].resolve([])
    automation_conf[CONF_AUTOMATION_ID].resolve([])
    automation_conf[CONF_THEN][0][CONF_TYPE_ID].resolve([])

    seen_on_add_counts = []
    real_create = Widget.create

    def spy_create(name, var, wtype, config=None):
        seen_on_add_counts.append(len(_get_list_triggers(name).on_add))
        return real_create(name, var, wtype, config)

    parent = MockObj("parent_obj")
    with patch.object(Widget, "create", side_effect=spy_create):
        async with LvContext():
            await widget_to_code(config, list_spec, parent)

    assert seen_on_add_counts == [1], (
        "on_add wasn't recorded yet when Widget.create() registered the list"
    )
