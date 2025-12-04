from collections.abc import Callable
from typing import Any

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ACTION, CONF_GROUP, CONF_ID, CONF_TIMEOUT
from esphome.core import Lambda
from esphome.cpp_generator import TemplateArguments, get_variable
from esphome.cpp_types import nullptr

from .defines import (
    CONF_BG_OPA,
    CONF_BOTTOM_LAYER,
    CONF_EDITING,
    CONF_FREEZE,
    CONF_LVGL_ID,
    CONF_MAIN,
    CONF_OBJ,
    CONF_SCROLLBAR,
    CONF_SHOW_SNOW,
    CONF_TOP_LAYER,
    PARTS,
    StaticCastExpression,
    add_warning,
)
from .lv_validation import lv_bool, lv_milliseconds
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
    part_schema,
)
from .types import (
    LV_STATE,
    LvglAction,
    LvglCondition,
    ObjUpdateAction,
    lv_group_t,
    lv_obj_base_t,
    lv_obj_t,
    lv_pseudo_button_t,
)
from .widgets import (
    Widget,
    WidgetType,
    add_widgets,
    get_screen_active,
    get_widgets,
    set_obj_properties,
    wait_for_widgets,
)

# Record widgets that are used in a focused action here
focused_widgets = set()
refreshed_widgets = set()


async def layers_to_code(lv_component, config):
    if top_conf := config.get(CONF_TOP_LAYER):
        top_layer = lv_expr.display_get_layer_top(lv_component.get_disp())
        with LocalVariable("top_layer", lv_obj_t, top_layer) as top_layer_obj:
            top_w = Widget(top_layer_obj, layer_spec, top_conf)
            await set_obj_properties(top_w, top_conf)
            await add_widgets(top_w, top_conf)
    if bottom_conf := config.get(CONF_BOTTOM_LAYER):
        bottom_layer = lv_expr.display_get_layer_bottom(lv_component.get_disp())
        with LocalVariable("bottom_layer", lv_obj_t, bottom_layer) as bottom_layer_obj:
            bottom_w = Widget(bottom_layer_obj, layer_spec, bottom_conf)
            await set_obj_properties(bottom_w, bottom_conf)
            await add_widgets(bottom_w, bottom_conf)


async def lvgl_update(lv_component, config):
    bottom = {k.removeprefix("disp_"): v for k, v in config.items() if k in DISP_PROPS}
    if not bottom:
        return
    plural = len(bottom) != 1
    add_warning(
        "The propert"
        + ("ies " if plural else "y ")
        + "'"
        + "','".join(k for k in config if k in DISP_PROPS)
        + "'"
        + (" are " if plural else " is ")
        + "deprecated, use 'bottom_layer' instead."
    )
    # Preserve default opacity from 8.x
    if CONF_BG_OPA not in bottom:
        bottom[CONF_BG_OPA] = 1.0
    await layers_to_code(lv_component, {CONF_BOTTOM_LAYER: bottom})


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
    return cg.new_Pvariable(action_id, template_arg, await context.get_lambda())


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
                lv_milliseconds,
            )
        }
    ),
)
async def lvgl_is_idle(config, condition_id, template_arg, args):
    lvgl = config[CONF_LVGL_ID]
    timeout = await lv_milliseconds.process(config[CONF_TIMEOUT])
    async with LambdaContext(LVGL_COMP_ARG, return_type=cg.bool_) as context:
        lv_add(
            ReturnStatement(
                lv_expr.disp_get_inactive_time(lvgl_comp.get_disp()) > timeout
            )
        )
    var = cg.new_Pvariable(
        condition_id,
        TemplateArguments(LvglComponent, *template_arg),
        await context.get_lambda(),
    )
    await cg.register_parented(var, lvgl)
    return var


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
        widgets = [get_screen_active(lv_comp)]
    else:
        widgets = await get_widgets(config)

    async def do_invalidate(widget: Widget):
        lv_obj.invalidate(widget.obj)

    return await action_to_code(widgets, do_invalidate, action_id, template_arg, args)


layer_spec = WidgetType(CONF_OBJ, lv_obj_t, (CONF_MAIN, CONF_SCROLLBAR), is_mock=True)

DISP_PROPS = {str(x) for x in DISP_BG_SCHEMA.schema}


@automation.register_action(
    "lvgl.update",
    LvglAction,
    part_schema(layer_spec.parts)
    .extend(LVGL_SCHEMA)
    .extend(DISP_BG_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_TOP_LAYER): part_schema(layer_spec.parts),
            cv.Optional(CONF_BOTTOM_LAYER): part_schema(layer_spec.parts),
        }
    ),
)
async def lvgl_update_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config, CONF_LVGL_ID)
    w = widgets[0]
    async with LambdaContext(LVGL_COMP_ARG, where=action_id) as context:
        await lvgl_update(w.var, config)
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
        group = StaticCastExpression(
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
        return cg.new_Pvariable(action_id, template_arg, await context.get_lambda())


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
        # Check if v is a Lambda or a dict, implying it is dynamic
        if any(isinstance(v, (Lambda, dict)) for v in config.values()):
            await widget.type.to_code(widget, config)
            if (
                widget.type.w_type.value_property is not None
                and widget.type.w_type.value_property in config
            ):
                lv.event_send(widget.obj, UPDATE_EVENT, nullptr)

    return await action_to_code(widget, do_refresh, action_id, template_arg, args)
