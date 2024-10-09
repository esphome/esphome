import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.core import ID
from esphome.cpp_generator import MockObj

from .defines import (
    CONF_STYLE_DEFINITIONS,
    CONF_THEME,
    CONF_TOP_LAYER,
    LValidator,
    literal,
)
from .helpers import add_lv_use
from .lvcode import LambdaContext, LocalVariable, lv, lv_assign, lv_variable
from .schemas import ALL_STYLES, STYLE_REMAP
from .types import lv_lambda_t, lv_obj_t, lv_obj_t_ptr
from .widgets import Widget, add_widgets, set_obj_properties, theme_widget_map
from .widgets.obj import obj_spec

TOP_LAYER = literal("lv_disp_get_layer_top(lv_component->get_disp())")


async def styles_to_code(config):
    """Convert styles to C__ code."""
    for style in config.get(CONF_STYLE_DEFINITIONS, ()):
        svar = cg.new_Pvariable(style[CONF_ID])
        lv.style_init(svar)
        for prop, validator in ALL_STYLES.items():
            if (value := style.get(prop)) is not None:
                if isinstance(validator, LValidator):
                    value = await validator.process(value)
                if isinstance(value, list):
                    value = "|".join(value)
                remapped_prop = STYLE_REMAP.get(prop, prop)
                lv.call(f"style_set_{remapped_prop}", svar, literal(value))


async def theme_to_code(config):
    if theme := config.get(CONF_THEME):
        add_lv_use(CONF_THEME)
        for w_name, style in theme.items():
            if not isinstance(style, dict):
                continue

            lname = "lv_theme_apply_" + w_name
            apply = lv_variable(lv_lambda_t, lname)
            theme_widget_map[w_name] = apply
            ow = Widget.create("obj", MockObj(ID("obj")), obj_spec)
            async with LambdaContext([(lv_obj_t_ptr, "obj")], where=w_name) as context:
                await set_obj_properties(ow, style)
            lv_assign(apply, await context.get_lambda())


async def add_top_layer(config):
    if top_conf := config.get(CONF_TOP_LAYER):
        with LocalVariable("top_layer", lv_obj_t, TOP_LAYER) as top_layer_obj:
            top_w = Widget(top_layer_obj, obj_spec, top_conf)
            await set_obj_properties(top_w, top_conf)
            await add_widgets(top_w, top_conf)
