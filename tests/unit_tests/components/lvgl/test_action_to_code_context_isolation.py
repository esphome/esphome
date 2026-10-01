"""Regression test: action_to_code(): Confirms two
actions whose own processing each suspends mid-lambda, waiting for an ID, don't
corrupt each other's LambdaContext when interleaved.
"""

from __future__ import annotations

import pytest

from esphome.automation import ACTION_REGISTRY
import esphome.codegen as cg
from esphome.components.lvgl.automation import action_to_code
from esphome.components.lvgl.lvcode import lv_add
from esphome.core import CORE, ID
from esphome.cpp_generator import RawExpression, TemplateArguments


@pytest.mark.asyncio
async def test_action_to_code_survives_interleaved_suspended_contexts(
    setup_core,
) -> None:
    later_id_a = ID("later_var_a", False, cg.int_)
    later_id_b = ID("later_var_b", False, cg.int_)
    action_type = ACTION_REGISTRY["lvgl.list.add"].type_id

    async def action_a(_widget) -> None:
        value = await cg.get_variable(later_id_a)
        lv_add(RawExpression(f"action_a_marker({value})"))

    async def action_b(_widget) -> None:
        value = await cg.get_variable(later_id_b)
        lv_add(RawExpression(f"action_b_marker({value})"))

    async def run_action_a() -> None:
        action_id = ID("test_action_a", is_declaration=True, type=action_type)
        await action_to_code([None], action_a, action_id, TemplateArguments(), [])

    async def run_action_b() -> None:
        action_id = ID("test_action_b", is_declaration=True, type=action_type)
        await action_to_code([None], action_b, action_id, TemplateArguments(), [])

    async def define_later_ids() -> None:
        # Both actions are still suspended, mid-LambdaContext, when this runs -
        # resolving both at once lets each resume while the other's context is
        # still open, rather than one finishing before the other starts.
        cg.new_variable(later_id_a, RawExpression("1"))
        cg.new_variable(later_id_b, RawExpression("2"))

    CORE.add_job(run_action_a)
    CORE.add_job(run_action_b)
    CORE.add_job(define_later_ids)
    CORE.flush_tasks()

    text = "\n".join(str(s) for s in CORE.main_statements)
    assert "action_a_marker(later_var_a)" in text
    assert "action_b_marker(later_var_b)" in text
