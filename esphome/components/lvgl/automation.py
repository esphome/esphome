from typing import Any, Callable

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ACTION, CONF_GROUP, CONF_ID, CONF_TIMEOUT
from esphome.core import Lambda
from esphome.cpp_generator import TemplateArguments, get_variable
from esphome.cpp_types import nullptr

from .defines import (
    CONF_DISP_BG_COLOR,
    CONF_DISP_BG_IMAGE,
    CONF_DISP_BG_OPA,
    CONF_EDITING,
    CONF_FREEZE,
    CONF_LVGL_ID,
    CONF_SHOW_SNOW,
    PARTS,
    literal,
    static_cast,
)
from .lv_validation import lv_bool, lv_color, lv_image, opacity
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
)
from .schemas import (
    ALL_STYLES,
    DISP_BG_SCHEMA,
    LIST_ACTION_SCHEMA,
    LVGL_SCHEMA,
    base_update_schema,
)
from .types import (
    LV_STATE,
    LvglAction,
    LvglCondition,
    ObjUpdateAction,
    lv_disp_t,
    lv_group_t,
    lv_obj_base_t,
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
refreshed_widgets = set()


async def action_to_code(
    widgets: list[Widget],
    action: Callable[[Widget], Any],
    action_id,
    template_arg,
    args,
    config=None,
):
    # Ensure all required ids have been processed, so our LambdaContext doesn't get context-switched.
    if config:
        for lamb in config.values():
            if isinstance(lamb, Lambda):
                for id_ in lamb.requires_ids:
                    await get_variable(id_)
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
    return await action_to_code(
        widgets, do_update, action_id, template_arg, args, config
    )


@automation.register_condition(
    "lvgl.is_paused",
    LvglCondition,
    LVGL_SCHEMA,
)
async def lvgl_is_paused(config, condition_id, template_arg, args):
    lvgl = config[CONF_LVGL_ID]
    async with LambdaContext(LVGL_COMP_ARG, return_type=cg.bool_) as context:
        lv_add(ReturnStatement(lvgl_comp.is_paused()))
    var = cg.new_Pvariable(
        condition_id,
        TemplateArguments(LvglComponent, *template_arg),
        await context.get_lambda(),
    )
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
    var = cg.new_Pvariable(
        condition_id,
        TemplateArguments(LvglComponent, *template_arg),
        await context.get_lambda(),
    )
    await cg.register_parented(var, lvgl)
    return var


async def disp_update(disp, config: dict):
    if (
        CONF_DISP_BG_COLOR not in config
        and CONF_DISP_BG_IMAGE not in config
        and CONF_DISP_BG_OPA not in config
    ):
        return
    with LocalVariable("lv_disp_tmp", lv_disp_t, disp) as disp_temp:
        if (bg_color := config.get(CONF_DISP_BG_COLOR)) is not None:
            lv.disp_set_bg_color(disp_temp, await lv_color.process(bg_color))
        if bg_image := config.get(CONF_DISP_BG_IMAGE):
            if bg_image == "none":
                lv.disp_set_bg_image(disp_temp, static_cast("void *", "nullptr"))
            else:
                lv.disp_set_bg_image(disp_temp, await lv_image.process(bg_image))
        if (bg_opa := config.get(CONF_DISP_BG_OPA)) is not None:
            lv.disp_set_bg_opa(disp_temp, await opacity.process(bg_opa))


@automation.register_action(
    "lvgl.widget.redraw",
    ObjUpdateAction,
    cv.Any(
        cv.maybe_simple_value(
            {
                cv.Required(CONF_ID): cv.use_id(lv_obj_t),
            },
            key=CONF_ID,
        ),
        LVGL_SCHEMA,
    ),
)
async def obj_invalidate_to_code(config, action_id, template_arg, args):
    if CONF_LVGL_ID in config:
        lv_comp = await cg.get_variable(config[CONF_LVGL_ID])
        widgets = [get_scr_act(lv_comp)]
    else:
        widgets = await get_widgets(config)

    async def do_invalidate(widget: Widget):
        lv_obj.invalidate(widget.obj)

    return await action_to_code(widgets, do_invalidate, action_id, template_arg, args)


@automation.register_action(
    "lvgl.update",
    LvglAction,
    DISP_BG_SCHEMA.extend(LVGL_SCHEMA).add_extra(
        cv.has_at_least_one_key(CONF_DISP_BG_COLOR, CONF_DISP_BG_IMAGE)
    ),
)
async def lvgl_update_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config, CONF_LVGL_ID)
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
    LVGL_SCHEMA.extend(
        {
            cv.Optional(CONF_SHOW_SNOW, default=False): lv_bool,
        }
    ),
)
async def pause_action_to_code(config, action_id, template_arg, args):
    lv_comp = await cg.get_variable(config[CONF_LVGL_ID])
    async with LambdaContext(LVGL_COMP_ARG) as context:
        add_line_marks(where=action_id)
        lv_add(lvgl_comp.set_paused(True, config[CONF_SHOW_SNOW]))
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, lv_comp)
    return var


@automation.register_action(
    "lvgl.resume",
    LvglAction,
    LVGL_SCHEMA,
)
async def resume_action_to_code(config, action_id, template_arg, args):
    lv_comp = await cg.get_variable(config[CONF_LVGL_ID])
    async with LambdaContext(LVGL_COMP_ARG, where=action_id) as context:
        lv_add(lvgl_comp.set_paused(False, False))
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, lv_comp)
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
            LVGL_SCHEMA.extend(
                {
                    cv.Optional(CONF_GROUP): cv.use_id(lv_group_t),
                    cv.Required(CONF_ACTION): cv.one_of(
                        "MARK", "RESTORE", "NEXT", "PREVIOUS", upper=True
                    ),
                    cv.Optional(CONF_FREEZE, default=False): cv.boolean,
                }
            ),
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


@automation.register_action(
    "lvgl.widget.update", ObjUpdateAction, base_update_schema(lv_obj_base_t, PARTS)
)
async def obj_update_to_code(config, action_id, template_arg, args):
    async def do_update(widget: Widget):
        await set_obj_properties(widget, config)

    widgets = await get_widgets(config[CONF_ID])
    return await action_to_code(
        widgets, do_update, action_id, template_arg, args, config
    )


def validate_refresh_config(config):
    for w in config:
        refreshed_widgets.add(w[CONF_ID])
    return config


@automation.register_action(
    "lvgl.widget.refresh",
    ObjUpdateAction,
    cv.All(
        cv.ensure_list(
            cv.maybe_simple_value(
                {
                    cv.Required(CONF_ID): cv.use_id(lv_obj_t),
                },
                key=CONF_ID,
            )
        ),
        validate_refresh_config,
    ),
)
async def obj_refresh_to_code(config, action_id, template_arg, args):
    widget = await get_widgets(config)

    async def do_refresh(widget: Widget):
        # only update style properties that might have changed, i.e. are templated
        config = {k: v for k, v in widget.config.items() if isinstance(v, Lambda)}
        await set_obj_properties(widget, config)
        # must pass all widget-specific options here, even if not templated, but only do so if at least one is
        # templated. First filter out common style properties.
        config = {k: v for k, v in widget.config.items() if k not in ALL_STYLES}
        if any(isinstance(v, Lambda) for v in config.values()):
            await widget.type.to_code(widget, config)
            if (
                widget.type.w_type.value_property is not None
                and widget.type.w_type.value_property in config
            ):
                lv.event_send(widget.obj, UPDATE_EVENT, nullptr)

    return await action_to_code(widget, do_refresh, action_id, template_arg, args)
