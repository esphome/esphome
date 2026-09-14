from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_DEFAULT, CONF_ID
from esphome.core import ID
from esphome.cpp_generator import MockObj

from .defines import (
    CONF_STYLE_DEFINITIONS,
    CONF_THEME,
    PARTS,
    STATES,
    LValidator,
    add_lv_use,
    get_part_state_selector,
    get_styles_used,
    get_theme_styles,
    get_theme_update_requests,
    get_widget_theme_style_data,
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
from .widgets import collect_parts


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
    style = await cg.get_variable(config[CONF_ID])
    async with LambdaContext(parameters=args, where=action_id) as context:
        await style_set(style, config)
        # Refresh and redraw every widget using this style -- otherwise the
        # updated properties would sit unused until something else happens to
        # invalidate the affected widgets.
        lv.obj_report_style_change(style)

    return cg.new_Pvariable(action_id, template_arg, await context.get_lambda())


def _get_theme_style_name(w_name: str, part: str, state: str) -> str:
    return f"_lv_theme_style_{w_name}_{part}_{state}"


def get_widget_theme_styles(w_name: str) -> list[tuple[MockObj, MockObj]]:
    """Return a list of (style variable, part/state name) for all theme styles used by the given widget type."""
    widget_styles = get_widget_theme_style_data()
    if w_name in widget_styles:
        return widget_styles[w_name]
    theme_styles = get_theme_styles()
    style_list = []
    for part in PARTS:
        for state in STATES + (CONF_DEFAULT,):
            style_name = _get_theme_style_name(w_name, part, state)
            if style_name in theme_styles:
                style_list.append(
                    (theme_styles[style_name], get_part_state_selector(part, state))
                )
    widget_styles[w_name] = style_list
    return style_list


async def theme_to_code(config):
    """
    Convert theme to C++ code. May be called multiple times for different LVGL instances.
    A style is created for each (widget type, part, state) combo declared in the `theme:` section of the config,
    or requested by a `theme.update` action.
    If a style is requested but not declared, it is created as an empty placeholder.
    :param config:
    :return:
    """
    theme = config.get(CONF_THEME) or {}
    requests = get_theme_update_requests()
    widget_names = [
        w_name for w_name in WIDGET_TYPES if w_name in theme or w_name in requests
    ]
    if not widget_names:
        return
    add_lv_use(CONF_THEME)
    style_map = get_theme_styles()
    for w_name in widget_names:
        declared_parts = collect_parts(theme[w_name]) if w_name in theme else {}
        parts = {part: dict(states) for part, states in declared_parts.items()}
        for part, state in requests.get(w_name, {}):
            parts.setdefault(part, {}).setdefault(state, {})
        for part, states in parts.items():
            declared_states = declared_parts.get(part, {})
            for state, props in states.items():
                style_name = _get_theme_style_name(w_name, part, state)
                if style_name not in style_map:
                    style_map[style_name] = await create_style(
                        _get_theme_style_name(w_name, part, state), props
                    )
                elif state in declared_states:
                    # A `theme.update` request for this combo (possibly from
                    # another LVGL instance) already created the style as an
                    # empty placeholder before this instance's real `theme:`
                    # declaration was reached -- apply the real values now
                    # instead of silently leaving it empty.
                    await style_set(style_map[style_name], props)


@automation.register_action(
    "lvgl.theme.update",
    ObjUpdateAction,
    theme_update_schema,
    synchronous=True,
)
async def theme_update_to_code(config, action_id, template_arg, args) -> MockObj:
    # The theme_update_schema records the requested (widget type, part, state) combos in a global dict so that
    # theme_to_code() can create the corresponding styles variables. Here we await get_variable(), which will
    # context switch if required so theme_to_code() can run and create the style variable.
    to_update: list[tuple] = []
    for w_name, style in config.items():
        for part, states in collect_parts(style).items():
            for state, props in states.items():
                # Skip states with no properties to set.
                if not props:
                    continue
                style_var = await cg.get_variable(
                    ID(_get_theme_style_name(w_name, part, state))
                )
                to_update.append((style_var, props))
    async with LambdaContext(parameters=args, where=action_id) as context:
        for style_var, props in to_update:
            await style_set(style_var, props)
            # Trigger a redraw for affected widgets.
            lv.obj_report_style_change(style_var)

    return cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
