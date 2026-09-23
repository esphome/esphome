from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import ID

from .defines import (
    CONF_STYLE_DEFINITIONS,
    CONF_THEME,
    LValidator,
    add_lv_use,
    get_styles_used,
    get_theme_update_requests,
    get_theme_widget_map,
    literal,
)
from .lvcode import LambdaContext, lv
from .schemas import (
    ALL_STYLES,
    FULL_STYLE_SCHEMA,
    WIDGET_TYPES,
    remap_property,
    theme_update_schema,
)
from .types import ObjUpdateAction, lv_style_t
from .widgets import collect_parts, wait_for_widgets


def has_style_props(config) -> bool:
    return any(prop in config for prop in ALL_STYLES)


async def style_set(svar, style):
    for prop, validator in ALL_STYLES.items():
        if (value := style.get(prop)) is not None:
            get_styles_used().add(prop)
            if isinstance(validator, LValidator):
                value = await validator.process(value)
            if isinstance(value, list):
                value = "|".join(value)
            lv.call(f"style_set_{remap_property(prop)}", svar, literal(value))


async def create_style(id_name, style=None):
    style_id = ID(id_name, True, lv_style_t)
    svar = cg.new_Pvariable(style_id)
    lv.style_init(svar)
    if style:
        await style_set(svar, style)
    return svar


class LVStyle:
    """
    A class to lazily create a named style
    """

    named_styles = {}

    def __init__(self, id_name, style=None):
        self.id_name = id_name
        self.style = style
        self._style_var = None

    async def get_var(self):
        if self._style_var is None:
            self._style_var = await create_style(self.id_name + "_style", self.style)
        return self._style_var

    @classmethod
    def get_style(cls, id_name):
        return cls.named_styles.setdefault(id_name, LVStyle(id_name))


async def styles_to_code(config):
    """Convert styles to C__ code."""
    for style in config.get(CONF_STYLE_DEFINITIONS, ()):
        await create_style(style[CONF_ID].id, style)


@automation.register_action(
    "lvgl.style.update",
    ObjUpdateAction,
    FULL_STYLE_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.use_id(lv_style_t),
        }
    ),
    synchronous=True,
)
async def style_update_to_code(config, action_id, template_arg, args):
    await wait_for_widgets()
    style = await cg.get_variable(config[CONF_ID])
    async with LambdaContext(parameters=args, where=action_id) as context:
        await style_set(style, config)
        # Refresh and redraw every widget using this style -- otherwise the
        # updated properties would sit unused until something else happens to
        # invalidate the affected widgets.
        lv.obj_report_style_change(style)

    return cg.new_Pvariable(action_id, template_arg, await context.get_lambda())


async def theme_to_code(config):
    theme = config.get(CONF_THEME) or {}
    requests = get_theme_update_requests()
    # Iterate in WIDGET_TYPES' (deterministic, registration-order) sequence rather
    # than a set -- a set of strings/tuples iterates in an order that depends on
    # per-process hash randomization, which would otherwise churn the order hidden
    # style variables are declared in main.cpp between builds of the same config.
    widget_names = [
        w_name for w_name in WIDGET_TYPES if w_name in theme or w_name in requests
    ]
    if not widget_names:
        return
    add_lv_use(CONF_THEME)
    theme_map = get_theme_widget_map()
    for w_name in widget_names:
        declared_parts = collect_parts(theme[w_name]) if w_name in theme else {}
        parts = {part: dict(states) for part, states in declared_parts.items()}
        for part, state in requests.get(w_name, {}):
            parts.setdefault(part, {}).setdefault(state, {})
        widget_styles = theme_map.setdefault(w_name, {})
        for part, states in parts.items():
            part_styles = widget_styles.setdefault(part, {})
            declared_states = declared_parts.get(part, {})
            for state, props in states.items():
                if state not in part_styles:
                    part_styles[state] = await create_style(
                        "_lv_theme_style_" + w_name + "_" + part + "_" + state, props
                    )
                elif state in declared_states:
                    # A `theme.update` request for this combo (possibly from
                    # another LVGL instance) already created the style as an
                    # empty placeholder before this instance's real `theme:`
                    # declaration was reached -- apply the real values now
                    # instead of silently leaving it empty.
                    await style_set(part_styles[state], props)


@automation.register_action(
    "lvgl.theme.update",
    ObjUpdateAction,
    theme_update_schema,
    synchronous=True,
)
async def theme_update_to_code(config, action_id, template_arg, args):
    await wait_for_widgets()
    theme_map = get_theme_widget_map()
    # Invariant this relies on: theme_update_schema() records every (widget
    # type, part, state) combo this action targets as a request during config
    # validation (which completes for the whole config tree before any
    # to_code runs), and theme_to_code() -- which runs for every LVGL
    # instance before any action's own to_code -- materialises a style for
    # each recorded request. If that handshake is ever broken by a future
    # change, fail with a diagnosable message rather than a bare KeyError.
    to_update = []
    for w_name, style in config.items():
        for part, states in collect_parts(style).items():
            for state, props in states.items():
                # collect_parts() unconditionally seeds an (empty) main/default
                # entry even when this action didn't target it -- skip it, both
                # because there's nothing to update and because
                # theme_update_schema no longer pre-creates a placeholder style
                # for combos with no properties.
                if not props:
                    continue
                style_var = theme_map.get(w_name, {}).get(part, {}).get(state)
                if style_var is None:
                    raise cv.Invalid(
                        f"No theme style exists for '{w_name}' {part}/{state}. "
                        "This is an internal error -- please report it."
                    )
                to_update.append((style_var, props))
    async with LambdaContext(parameters=args, where=action_id) as context:
        for style_var, props in to_update:
            await style_set(style_var, props)
            # Refresh and redraw every widget using this style -- otherwise the
            # updated properties would sit unused until something else happens
            # to invalidate the affected widgets.
            lv.obj_report_style_change(style_var)

    return cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
