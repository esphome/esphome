from collections.abc import Callable
from typing import Any

from esphome import automation
from esphome.automation import ApplyAction
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTION,
    CONF_GROUP,
    CONF_HIDDEN,
    CONF_ID,
    CONF_POSITION,
    CONF_ROTATION,
    CONF_TIMEOUT,
)
from esphome.core import Lambda
from esphome.cpp_generator import StaticCastExpression
from esphome.cpp_types import nullptr

from .defines import (
    CONF_BG_OPA,
    CONF_BOTTOM_LAYER,
    CONF_EDITING,
    CONF_FREEZE,
    CONF_LAYOUT,
    CONF_LVGL_ID,
    CONF_MAIN,
    CONF_OBJ,
    CONF_SCROLLBAR,
    CONF_SHOW_SNOW,
    CONF_TOP_LAYER,
    PARTS,
    add_warning,
    get_focused_widgets,
    get_options,
    get_refreshed_widgets,
    literal,
)
from .layout import layout_validator
from .lv_validation import lv_bool, lv_milliseconds, lv_rotation
from .lvcode import (
    UPDATE_EVENT,
    LambdaContext,
    LocalVariable,
    LvConditional,
    add_line_marks,
    context_action,
    lv,
    lv_add,
    lv_expr,
    lv_global,
    lv_obj,
    lvgl_parent,
)
from .schemas import (
    ALL_STYLES,
    DISP_BG_SCHEMA,
    LIST_ACTION_SCHEMA,
    LVGL_SCHEMA,
    base_update_schema,
    part_schema,
)
from .types import LV_STATE, lv_group_t, lv_obj_base_t, lv_obj_t, lv_pseudo_button_t
from .widgets import (
    Widget,
    WidgetType,
    add_widgets,
    get_screen_active,
    get_widgets,
    set_obj_properties,
)

# Widgets that are used in a focused/refreshed action are tracked in
# ``CORE.data`` (under the lvgl domain) so the state is cleared between
# successive compilations / unit tests via ``CORE.reset()``.


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
):
    async with LambdaContext(parameters=args, where=action_id) as context:
        for widget in widgets:
            await action(widget)
    return await context_action(context, action_id, template_arg, args)


async def update_to_code(config, action_id, template_arg, args):
    async def do_update(widget: Widget):
        await set_obj_properties(widget, config)
        await widget.type.update_to_code(widget, config)
        if (
            widget.type.w_type.value_property is not None
            and widget.type.w_type.value_property in config
        ):
            lv_obj.send_event(widget.obj, UPDATE_EVENT, nullptr)

    widgets = await get_widgets(config[CONF_ID])
    return await action_to_code(widgets, do_update, action_id, template_arg, args)


automation.register_apply_condition(
    "lvgl.is_paused", LVGL_SCHEMA, "is_paused()", id_key=CONF_LVGL_ID
)
automation.register_apply_condition(
    "lvgl.is_idle",
    LVGL_SCHEMA.extend(
        {
            cv.Required(CONF_TIMEOUT): cv.templatable(
                lv_milliseconds,
            )
        }
    ),
    automation.ApplyCall(
        "is_idle({})",
        ((CONF_TIMEOUT, cg.uint32, lambda _, v: str(v.total_milliseconds)),),
    ),
    id_key=CONF_LVGL_ID,
)


def _validate_rotation(value):
    # Note that we need rotation
    get_options()[CONF_ROTATION] = True
    return lv_rotation(value)


@automation.register_action(
    "lvgl.display.set_rotation",
    ApplyAction,
    cv.maybe_simple_value(
        LVGL_SCHEMA.extend(
            {
                cv.Required(CONF_ROTATION): _validate_rotation,
            }
        ),
        key=CONF_ROTATION,
    ),
    synchronous=True,
)
async def lvgl_set_rotation(config, action_id, template_arg, args):
    lv_comp = await lvgl_parent(config)
    async with LambdaContext(args, where=action_id) as context:
        rotation = await lv_rotation.process(config[CONF_ROTATION])
        lv_add(lv_comp.set_rotation(rotation))
    return await context_action(context, action_id, template_arg, args)


@automation.register_action(
    "lvgl.widget.redraw",
    ApplyAction,
    cv.Any(
        cv.maybe_simple_value(
            {
                cv.Required(CONF_ID): cv.use_id(lv_obj_t),
            },
            key=CONF_ID,
        ),
        LVGL_SCHEMA,
    ),
    synchronous=True,
)
async def obj_invalidate_to_code(config, action_id, template_arg, args):
    if CONF_LVGL_ID in config:
        lv_comp = await lvgl_parent(config)
        widgets = [get_screen_active(lv_comp)]
    else:
        widgets = await get_widgets(config)

    async def do_invalidate(widget: Widget):
        lv_obj.invalidate(widget.obj)

    return await action_to_code(widgets, do_invalidate, action_id, template_arg, args)


layer_spec = WidgetType(CONF_OBJ, lv_obj_t, (CONF_MAIN, CONF_SCROLLBAR), is_mock=True)

DISP_PROPS = {str(x) for x in DISP_BG_SCHEMA.schema}


def _layer_update_schema() -> cv.Schema:
    """Schema for updating a display layer's styling and layout options."""
    return part_schema(layer_spec.parts).extend(
        {cv.Optional(CONF_LAYOUT): layout_validator}
    )


@automation.register_action(
    "lvgl.update",
    ApplyAction,
    part_schema(layer_spec.parts)
    .extend(LVGL_SCHEMA)
    .extend(DISP_BG_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_LAYOUT): layout_validator,
            cv.Optional(CONF_TOP_LAYER): _layer_update_schema(),
            cv.Optional(CONF_BOTTOM_LAYER): _layer_update_schema(),
        }
    ),
    synchronous=True,
)
async def lvgl_update_to_code(config, action_id, template_arg, args):
    lv_comp = await lvgl_parent(config)
    async with LambdaContext(args, where=action_id) as context:
        # Apply the top-level properties (styles and layout) to the active screen...
        await set_obj_properties(get_screen_active(lv_comp), config)
        # ...the deprecated flat `disp_*` background properties...
        await lvgl_update(lv_comp, config)
        # ...and the `top_layer`/`bottom_layer` keys (styling and layout updates).
        await layers_to_code(lv_comp, config)
    return await context_action(context, action_id, template_arg, args)


@automation.register_action(
    "lvgl.pause",
    ApplyAction,
    LVGL_SCHEMA.extend(
        {
            cv.Optional(CONF_SHOW_SNOW, default=False): lv_bool,
        }
    ),
    synchronous=True,
)
async def pause_action_to_code(config, action_id, template_arg, args):
    async with LambdaContext(args) as context:
        add_line_marks(where=action_id)
        lv_add((await lvgl_parent(config)).set_paused(True, config[CONF_SHOW_SNOW]))
    return await context_action(context, action_id, template_arg, args)


@automation.register_action(
    "lvgl.resume",
    ApplyAction,
    LVGL_SCHEMA,
    synchronous=True,
)
async def resume_action_to_code(config, action_id, template_arg, args):
    async with LambdaContext(args, where=action_id) as context:
        lv_add((await lvgl_parent(config)).set_paused(False, False))
    return await context_action(context, action_id, template_arg, args)


@automation.register_action(
    "lvgl.widget.disable", ApplyAction, LIST_ACTION_SCHEMA, synchronous=True
)
async def obj_disable_to_code(config, action_id, template_arg, args):
    async def do_disable(widget: Widget):
        widget.set_state(LV_STATE.DISABLED, True)

    return await action_to_code(
        await get_widgets(config), do_disable, action_id, template_arg, args
    )


@automation.register_action(
    "lvgl.widget.enable", ApplyAction, LIST_ACTION_SCHEMA, synchronous=True
)
async def obj_enable_to_code(config, action_id, template_arg, args):
    async def do_enable(widget: Widget):
        widget.set_state(LV_STATE.DISABLED, False)

    return await action_to_code(
        await get_widgets(config), do_enable, action_id, template_arg, args
    )


@automation.register_action(
    "lvgl.widget.hide", ApplyAction, LIST_ACTION_SCHEMA, synchronous=True
)
async def obj_hide_to_code(config, action_id, template_arg, args):
    async def do_hide(widget: Widget):
        widget.set_flag(CONF_HIDDEN, True)

    widgets = [widget.outer or widget for widget in await get_widgets(config)]
    return await action_to_code(widgets, do_hide, action_id, template_arg, args)


@automation.register_action(
    "lvgl.widget.show", ApplyAction, LIST_ACTION_SCHEMA, synchronous=True
)
async def obj_show_to_code(config, action_id, template_arg, args):
    async def do_show(widget: Widget):
        widget.set_flag(CONF_HIDDEN, False)
        if widget.move_to_foreground:
            lv_obj.move_foreground(widget.obj)

    widgets = [widget.outer or widget for widget in await get_widgets(config)]
    return await action_to_code(widgets, do_show, action_id, template_arg, args)


SET_Z_INDEX_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.ensure_list(
            cv.maybe_simple_value(
                {cv.Required(CONF_ID): cv.use_id(lv_obj_t)},
                key=CONF_ID,
            )
        ),
        cv.Required(CONF_POSITION): cv.Any(
            cv.one_of("TOP", "BOTTOM", "UP", "DOWN", upper=True), cv.int_
        ),
    }
)


@automation.register_action(
    "lvgl.widget.set_z_index", ApplyAction, SET_Z_INDEX_SCHEMA, synchronous=True
)
async def obj_set_z_index_to_code(config, action_id, template_arg, args):
    position = config[CONF_POSITION]

    async def do_set_z_index(widget: Widget):
        if position == "TOP":
            lv_obj.move_foreground(widget.obj)
        elif position == "BOTTOM":
            lv_obj.move_background(widget.obj)
        elif position == "UP":
            lv_obj.move_to_index(
                widget.obj, literal(f"{lv_expr.obj_get_index(widget.obj)} + 1")
            )
        elif position == "DOWN":
            with LvConditional(literal(f"{lv_expr.obj_get_index(widget.obj)} > 0")):
                lv_obj.move_to_index(
                    widget.obj, literal(f"{lv_expr.obj_get_index(widget.obj)} - 1")
                )
        else:
            lv_obj.move_to_index(widget.obj, position)

    widgets = [widget.outer or widget for widget in await get_widgets(config[CONF_ID])]
    return await action_to_code(widgets, do_set_z_index, action_id, template_arg, args)


def focused_id(value):
    value = cv.use_id(lv_pseudo_button_t)(value)
    get_focused_widgets().add(value)
    return value


@automation.register_action(
    "lvgl.widget.focus",
    ApplyAction,
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
    synchronous=True,
)
async def widget_focus(config, action_id, template_arg, args):
    widget = await get_widgets(config)
    if widget:
        widget = widget[0]
        group = StaticCastExpression(
            lv_group_t.operator("ptr"), lv_expr.obj_get_group(widget.obj)
        )
    elif group := config.get(CONF_GROUP):
        group = await lv_global(group)
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
            lv_comp = await lvgl_parent(config)
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
    return await context_action(context, action_id, template_arg, args)


@automation.register_action(
    "lvgl.widget.update",
    ApplyAction,
    base_update_schema(lv_obj_base_t, PARTS),
    synchronous=True,
)
async def obj_update_to_code(config, action_id, template_arg, args):
    async def do_update(widget: Widget):
        await set_obj_properties(widget, config)

    widgets = await get_widgets(config[CONF_ID])
    return await action_to_code(widgets, do_update, action_id, template_arg, args)


def validate_refresh_config(config):
    refreshed = get_refreshed_widgets()
    for w in config:
        refreshed.add(w[CONF_ID])
    return config


@automation.register_action(
    "lvgl.widget.refresh",
    ApplyAction,
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
    synchronous=True,
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
                lv_obj.send_event(widget.obj, UPDATE_EVENT, nullptr)

    return await action_to_code(widget, do_refresh, action_id, template_arg, args)
