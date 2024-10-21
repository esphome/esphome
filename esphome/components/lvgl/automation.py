from collections.abc import Awaitable
from typing import Callable

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ACTION, CONF_GROUP, CONF_ID, CONF_TIMEOUT
from esphome.cpp_generator import get_variable
from esphome.cpp_types import nullptr

from .defines import (
    CONF_DISP_BG_COLOR,
    CONF_DISP_BG_IMAGE,
    CONF_EDITING,
    CONF_FREEZE,
    CONF_LVGL_ID,
    CONF_SHOW_SNOW,
    literal,
)
from .lv_validation import lv_bool, lv_color, lv_image
from .lvcode import (
    LVGL_COMP_ARG,
    UPDATE_EVENT,
    LambdaContext,
    LocalVariable,
    LvglComponent,
    ReturnStatement,
    add_line_marks,
    lv,
    lv_add,
    lv_expr,
    lv_obj,
    lvgl_comp,
    static_cast,
)
from .schemas import DISP_BG_SCHEMA, LIST_ACTION_SCHEMA, LVGL_SCHEMA
from .types import (
    LV_STATE,
    LvglAction,
    LvglCondition,
    ObjUpdateAction,
    lv_disp_t,
    lv_group_t,
    lv_obj_t,
    lv_pseudo_button_t,
)
from .widgets import (
    Widget,
    get_scr_act,
    get_widgets,
    set_obj_properties,
    wait_for_widgets,
)

# Record widgets that are used in a focused action here
focused_widgets = set()


async def action_to_code(
    widgets: list[Widget],
    action: Callable[[Widget], Awaitable[None]],
    action_id,
    template_arg,
    args,
):
    await wait_for_widgets()
    async with LambdaContext(parameters=args, where=action_id) as context:
        for widget in widgets:
            await action(widget)
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    return var


async def update_to_code(config, action_id, template_arg, args):
    async def do_update(widget: Widget):
        await set_obj_properties(widget, config)
        await widget.type.to_code(widget, config)
        if (
            widget.type.w_type.value_property is not None
            and widget.type.w_type.value_property in config
        ):
            lv.event_send(widget.obj, UPDATE_EVENT, nullptr)

    widgets = await get_widgets(config[CONF_ID])
    return await action_to_code(widgets, do_update, action_id, template_arg, args)


@automation.register_condition(
    "lvgl.is_paused",
    LvglCondition,
    LVGL_SCHEMA,
)
async def lvgl_is_paused(config, condition_id, template_arg, args):
    lvgl = config[CONF_LVGL_ID]
    async with LambdaContext(LVGL_COMP_ARG, return_type=cg.bool_) as context:
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
    async with LambdaContext(LVGL_COMP_ARG, return_type=cg.bool_) as context:
        lv_add(ReturnStatement(lvgl_comp.is_idle(timeout)))
    var = cg.new_Pvariable(condition_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, lvgl)
    return var


async def disp_update(disp, config: dict):
    if CONF_DISP_BG_COLOR not in config and CONF_DISP_BG_IMAGE not in config:
        return
    with LocalVariable("lv_disp_tmp", lv_disp_t, disp) as disp_temp:
        if (bg_color := config.get(CONF_DISP_BG_COLOR)) is not None:
            lv.disp_set_bg_color(disp_temp, await lv_color.process(bg_color))
        if bg_image := config.get(CONF_DISP_BG_IMAGE):
            lv.disp_set_bg_image(disp_temp, await lv_image.process(bg_image))


@automation.register_action(
    "lvgl.widget.redraw",
    ObjUpdateAction,
    cv.Any(
        cv.maybe_simple_value(
            {
                cv.Required(CONF_ID): cv.use_id(lv_obj_t),
                cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
            },
            key=CONF_ID,
        ),
        cv.Schema(
            {
                cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
            }
        ),
    ),
)
async def obj_invalidate_to_code(config, action_id, template_arg, args):
    lv_comp = await cg.get_variable(config[CONF_LVGL_ID])
    widgets = await get_widgets(config) or [get_scr_act(lv_comp)]

    async def do_invalidate(widget: Widget):
        lv_obj.invalidate(widget.obj)

    return await action_to_code(widgets, do_invalidate, action_id, template_arg, args)


@automation.register_action(
    "lvgl.update",
    LvglAction,
    DISP_BG_SCHEMA.extend(
        {
            cv.GenerateID(): cv.use_id(LvglComponent),
        }
    ).add_extra(cv.has_at_least_one_key(CONF_DISP_BG_COLOR, CONF_DISP_BG_IMAGE)),
)
async def lvgl_update_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config)
    w = widgets[0]
    disp = literal(f"{w.obj}->get_disp()")
    async with LambdaContext(LVGL_COMP_ARG, where=action_id) as context:
        await disp_update(disp, config)
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, w.var)
    return var


@automation.register_action(
    "lvgl.pause",
    LvglAction,
    {
        cv.GenerateID(): cv.use_id(LvglComponent),
        cv.Optional(CONF_SHOW_SNOW, default=False): lv_bool,
    },
)
async def pause_action_to_code(config, action_id, template_arg, args):
    async with LambdaContext(LVGL_COMP_ARG) as context:
        add_line_marks(where=action_id)
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
    async with LambdaContext(LVGL_COMP_ARG, where=action_id) as context:
        lv_add(lvgl_comp.set_paused(False, False))
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action("lvgl.widget.disable", ObjUpdateAction, LIST_ACTION_SCHEMA)
async def obj_disable_to_code(config, action_id, template_arg, args):
    async def do_disable(widget: Widget):
        widget.add_state(LV_STATE.DISABLED)

    return await action_to_code(
        await get_widgets(config), do_disable, action_id, template_arg, args
    )


@automation.register_action("lvgl.widget.enable", ObjUpdateAction, LIST_ACTION_SCHEMA)
async def obj_enable_to_code(config, action_id, template_arg, args):
    async def do_enable(widget: Widget):
        widget.clear_state(LV_STATE.DISABLED)

    return await action_to_code(
        await get_widgets(config), do_enable, action_id, template_arg, args
    )


@automation.register_action("lvgl.widget.hide", ObjUpdateAction, LIST_ACTION_SCHEMA)
async def obj_hide_to_code(config, action_id, template_arg, args):
    async def do_hide(widget: Widget):
        widget.add_flag("LV_OBJ_FLAG_HIDDEN")

    widgets = [
        widget.outer if widget.outer else widget for widget in await get_widgets(config)
    ]
    return await action_to_code(widgets, do_hide, action_id, template_arg, args)


@automation.register_action("lvgl.widget.show", ObjUpdateAction, LIST_ACTION_SCHEMA)
async def obj_show_to_code(config, action_id, template_arg, args):
    async def do_show(widget: Widget):
        widget.clear_flag("LV_OBJ_FLAG_HIDDEN")
        if widget.move_to_foreground:
            lv_obj.move_foreground(widget.obj)

    widgets = [
        widget.outer if widget.outer else widget for widget in await get_widgets(config)
    ]
    return await action_to_code(widgets, do_show, action_id, template_arg, args)


def focused_id(value):
    value = cv.use_id(lv_pseudo_button_t)(value)
    focused_widgets.add(value)
    return value


@automation.register_action(
    "lvgl.widget.focus",
    ObjUpdateAction,
    cv.Any(
        cv.maybe_simple_value(
            {
                cv.Optional(CONF_GROUP): cv.use_id(lv_group_t),
                cv.Required(CONF_ACTION): cv.one_of(
                    "MARK", "RESTORE", "NEXT", "PREVIOUS", upper=True
                ),
                cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
                cv.Optional(CONF_FREEZE, default=False): cv.boolean,
            },
            key=CONF_ACTION,
        ),
        cv.maybe_simple_value(
            {
                cv.Required(CONF_ID): focused_id,
                cv.Optional(CONF_FREEZE, default=False): cv.boolean,
                cv.Optional(CONF_EDITING, default=False): cv.boolean,
            },
            key=CONF_ID,
        ),
    ),
)
async def widget_focus(config, action_id, template_arg, args):
    widget = await get_widgets(config)
    if widget:
        widget = widget[0]
        group = static_cast(
            lv_group_t.operator("ptr"), lv_expr.obj_get_group(widget.obj)
        )
    elif group := config.get(CONF_GROUP):
        group = await get_variable(group)
    else:
        group = lv_expr.group_get_default()

    async with LambdaContext(parameters=args, where=action_id) as context:
        if widget:
            lv.group_focus_freeze(group, False)
            lv.group_focus_obj(widget.obj)
            if config[CONF_EDITING]:
                lv.group_set_editing(group, True)
        else:
            action = config[CONF_ACTION]
            lv_comp = await get_variable(config[CONF_LVGL_ID])
            if action == "MARK":
                context.add(lv_comp.set_focus_mark(group))
            else:
                lv.group_focus_freeze(group, False)
                if action == "RESTORE":
                    context.add(lv_comp.restore_focus_mark(group))
                elif action == "NEXT":
                    lv.group_focus_next(group)
                else:
                    lv.group_focus_prev(group)

        if config[CONF_FREEZE]:
            lv.group_focus_freeze(group, True)
        var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
        return var
