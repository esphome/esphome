from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import ID

from .defines import (
    CONF_STYLE_DEFINITIONS,
    CONF_THEME,
    CONF_TOP_LAYER,
    LValidator,
    literal,
)
from .helpers import add_lv_use
from .lvcode import LambdaContext, LocalVariable, lv
from .schemas import ALL_STYLES, FULL_STYLE_SCHEMA, STYLE_REMAP
from .types import ObjUpdateAction, lv_obj_t, lv_style_t
from .widgets import (
    Widget,
    add_widgets,
    collect_parts,
    set_obj_properties,
    theme_widget_map,
    wait_for_widgets,
)
from .widgets.obj import obj_spec


async def style_set(svar, style):
    for prop, validator in ALL_STYLES.items():
        if (value := style.get(prop)) is not None:
            if isinstance(validator, LValidator):
                value = await validator.process(value)
            if isinstance(value, list):
                value = "|".join(value)
            remapped_prop = STYLE_REMAP.get(prop, prop)
            lv.call(f"style_set_{remapped_prop}", svar, literal(value))


async def create_style(style, id_name):
    style_id = ID(id_name, True, lv_style_t)
    svar = cg.new_Pvariable(style_id)
    lv.style_init(svar)
    await style_set(svar, style)
    return svar


async def styles_to_code(config):
    """Convert styles to C__ code."""
    for style in config.get(CONF_STYLE_DEFINITIONS, ()):
        await create_style(style, style[CONF_ID].id)


@automation.register_action(
    "lvgl.style.update",
    ObjUpdateAction,
    FULL_STYLE_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.use_id(lv_style_t),
        }
    ),
)
async def style_update_to_code(config, action_id, template_arg, args):
    await wait_for_widgets()
    style = await cg.get_variable(config[CONF_ID])
    async with LambdaContext(parameters=args, where=action_id) as context:
        await style_set(style, config)

    return cg.new_Pvariable(action_id, template_arg, await context.get_lambda())


async def theme_to_code(config):
    if theme := config.get(CONF_THEME):
        add_lv_use(CONF_THEME)
        for w_name, style in theme.items():
            # Work around Python 3.10 bug with nested async comprehensions
            # With Python 3.11 this could be simplified
            # TODO: Now that we require Python 3.11+, this can be updated to use nested comprehensions
            styles = {}
            for part, states in collect_parts(style).items():
                styles[part] = {
                    state: await create_style(
                        props,
                        "_lv_theme_style_" + w_name + "_" + part + "_" + state,
                    )
                    for state, props in states.items()
                }
            theme_widget_map[w_name] = styles


async def add_top_layer(lv_component, config):
    top_layer = lv.disp_get_layer_top(lv_component.get_disp())
    if top_conf := config.get(CONF_TOP_LAYER):
        with LocalVariable("top_layer", lv_obj_t, top_layer) as top_layer_obj:
            top_w = Widget(top_layer_obj, obj_spec, top_conf)
            await set_obj_properties(top_w, top_conf)
            await add_widgets(top_w, top_conf)
