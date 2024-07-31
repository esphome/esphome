from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIMEOUT
from esphome.core import Lambda
from esphome.cpp_generator import RawStatement
from esphome.cpp_types import nullptr

from .defines import CONF_LVGL_ID, CONF_SHOW_SNOW, literal
from .lv_validation import lv_bool
from .lvcode import (
    LambdaContext,
    ReturnStatement,
    add_line_marks,
    lv,
    lv_add,
    lv_obj,
    lvgl_comp,
)
from .schemas import ACTION_SCHEMA, LVGL_SCHEMA
from .types import (
    LvglAction,
    LvglComponent,
    LvglComponentPtr,
    LvglCondition,
    ObjUpdateAction,
    lv_obj_t,
)
from .widget import Widget, get_widget, lv_scr_act, set_obj_properties


async def action_to_code(action: list, action_id, widget: Widget, template_arg, args):
    with LambdaContext() as context:
        lv.cond_if(widget.obj == nullptr)
        lv_add(RawStatement("  return;"))
        lv.cond_endif()
    code = context.get_code()
    code.extend(action)
    action = "\n".join(code) + "\n\n"
    lamb = await cg.process_lambda(Lambda(action), args)
    var = cg.new_Pvariable(action_id, template_arg, lamb)
    return var


async def update_to_code(config, action_id, template_arg, args):
    if config is not None:
        widget = await get_widget(config)
        with LambdaContext() as context:
            add_line_marks(action_id)
            await set_obj_properties(widget, config)
            await widget.type.to_code(widget, config)
            if (
                widget.type.w_type.value_property is not None
                and widget.type.w_type.value_property in config
            ):
                lv.event_send(widget.obj, literal("LV_EVENT_VALUE_CHANGED"), nullptr)
        return await action_to_code(
            context.get_code(), action_id, widget, template_arg, args
        )


@automation.register_condition(
    "lvgl.is_paused",
    LvglCondition,
    LVGL_SCHEMA,
)
async def lvgl_is_paused(config, condition_id, template_arg, args):
    lvgl = config[CONF_LVGL_ID]
    with LambdaContext(
        [(LvglComponentPtr, "lvgl_comp")], return_type=cg.bool_
    ) as context:
        lv_add(ReturnStatement(lvgl_comp.is_paused()))
    var = cg.new_Pvariable(condition_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, lvgl)
    return var


@automation.register_condition(
    "lvgl.is_idle",
    LvglCondition,
    LVGL_SCHEMA.extend(
        {
            cv.Required(CONF_TIMEOUT): cv.templatable(
                cv.positive_time_period_milliseconds
            )
        }
    ),
)
async def lvgl_is_idle(config, condition_id, template_arg, args):
    lvgl = config[CONF_LVGL_ID]
    timeout = await cg.templatable(config[CONF_TIMEOUT], [], cg.uint32)
    with LambdaContext(
        [(LvglComponentPtr, "lvgl_comp")], return_type=cg.bool_
    ) as context:
        lv_add(ReturnStatement(lvgl_comp.is_idle(timeout)))
    var = cg.new_Pvariable(condition_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, lvgl)
    return var


@automation.register_action(
    "lvgl.widget.redraw",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.Optional(CONF_ID): cv.use_id(lv_obj_t),
            cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
        }
    ),
)
async def obj_invalidate_to_code(config, action_id, template_arg, args):
    if CONF_ID in config:
        w = await get_widget(config)
    else:
        w = lv_scr_act
    with LambdaContext() as context:
        add_line_marks(action_id)
        lv_obj.invalidate(w.obj)
    return await action_to_code(context.get_code(), action_id, w, template_arg, args)


@automation.register_action(
    "lvgl.pause",
    LvglAction,
    {
        cv.GenerateID(): cv.use_id(LvglComponent),
        cv.Optional(CONF_SHOW_SNOW, default=False): lv_bool,
    },
)
async def pause_action_to_code(config, action_id, template_arg, args):
    with LambdaContext([(LvglComponentPtr, "lvgl_comp")]) as context:
        add_line_marks(action_id)
        lv_add(lvgl_comp.set_paused(True, config[CONF_SHOW_SNOW]))
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "lvgl.resume",
    LvglAction,
    {
        cv.GenerateID(): cv.use_id(LvglComponent),
    },
)
async def resume_action_to_code(config, action_id, template_arg, args):
    with LambdaContext([(LvglComponentPtr, "lvgl_comp")]) as context:
        add_line_marks(action_id)
        lv_add(lvgl_comp.set_paused(False, False))
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action("lvgl.widget.disable", ObjUpdateAction, ACTION_SCHEMA)
async def obj_disable_to_code(config, action_id, template_arg, args):
    w = await get_widget(config)
    with LambdaContext() as context:
        add_line_marks(action_id)
        w.add_state("LV_STATE_DISABLED")
    return await action_to_code(context.get_code(), action_id, w, template_arg, args)


@automation.register_action("lvgl.widget.enable", ObjUpdateAction, ACTION_SCHEMA)
async def obj_enable_to_code(config, action_id, template_arg, args):
    w = await get_widget(config)
    with LambdaContext() as context:
        add_line_marks(action_id)
        w.clear_state("LV_STATE_DISABLED")
    return await action_to_code(context.get_code(), action_id, w, template_arg, args)


@automation.register_action("lvgl.widget.hide", ObjUpdateAction, ACTION_SCHEMA)
async def obj_hide_to_code(config, action_id, template_arg, args):
    w = await get_widget(config)
    with LambdaContext() as context:
        add_line_marks(action_id)
        w.add_flag("LV_OBJ_FLAG_HIDDEN")
    return await action_to_code(context.get_code(), action_id, w, template_arg, args)


@automation.register_action("lvgl.widget.show", ObjUpdateAction, ACTION_SCHEMA)
async def obj_show_to_code(config, action_id, template_arg, args):
    w = await get_widget(config)
    with LambdaContext() as context:
        add_line_marks(action_id)
        w.clear_flag("LV_OBJ_FLAG_HIDDEN")
    return await action_to_code(context.get_code(), action_id, w, template_arg, args)
