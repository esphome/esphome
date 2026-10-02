"""Regression test: an lvgl.theme.update action must still apply its style
change even if it runs before theme_to_code() has built that style.

theme_update_to_code() reads get_theme_widget_map() synchronously and raises
cv.Invalid if the requested style isn't there yet - relying on theme_to_code()
(which materialises a style for every requested widget/part/state combo) to
have always already run. That's guaranteed when the action lives inside the
lvgl: block's own automations (same to_code() job, sequential), but not when
it's scheduled as part of a different component's own to_code() job - e.g.
tests/components/lvgl/lvgl-package.yaml's `esphome: on_boot:` case, which this
test reproduces at the scheduler level.
"""

from __future__ import annotations

import pytest

from esphome.automation import ACTION_REGISTRY
from esphome.components.lvgl.schemas import theme_update_schema
from esphome.components.lvgl.styles import theme_to_code
from esphome.core import CORE, ID
from esphome.cpp_generator import TemplateArguments


@pytest.mark.asyncio
async def test_theme_update_before_theme_to_code_still_applies(setup_core) -> None:
    add_config = theme_update_schema({"obj": {"border_width": 2}})

    entry = ACTION_REGISTRY["lvgl.theme.update"]
    action_id = ID("test_theme_update_action", is_declaration=True, type=entry.type_id)

    async def run_update_action() -> None:
        await entry.coroutine_fun(add_config, action_id, TemplateArguments(), [])

    # Scheduled before theme_to_code()'s job - mirrors the action being reached
    # from a different component's own to_code() job than lvgl's.
    CORE.add_job(run_update_action)
    CORE.add_job(theme_to_code, {})
    CORE.flush_tasks()

    statements = [str(s) for s in CORE.main_statements]
    assert any("style_set_border_width" in s for s in statements), (
        "theme.update's border_width change was never applied"
    )
