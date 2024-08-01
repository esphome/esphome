import esphome.codegen as cg
from esphome.const import CONF_ID

from .defines import (
    CONF_STYLE_DEFINITIONS,
    CONF_THEME,
    CONF_TOP_LAYER,
    LValidator,
    literal,
)
from .helpers import add_lv_use
from .lvcode import LambdaContext, LocalVariable, lv, lv_assign, lv_Pvariable
from .schemas import ALL_STYLES
from .types import lv_lambda_t, lv_obj_t
from .widget import Widget, add_widgets, set_obj_properties, theme_widget_map


async def styles_to_code(config):
    """Convert styles to C__ code."""
    for style in config.get(CONF_STYLE_DEFINITIONS, ()):
        svar = cg.new_Pvariable(style[CONF_ID])
        lv.style_init(svar)
        for prop, validator in ALL_STYLES.items():
            if value := style.get(prop):
                if isinstance(validator, LValidator):
                    value = await validator.process(value)
                if isinstance(value, list):
                    value = "|".join(value)
                lv.call(svar, f"style_set_{prop}", literal(value))


async def theme_to_code(config):
    if theme := config.get(CONF_THEME):
        add_lv_use(CONF_THEME)
        for w_name, style in theme.items():
            if not isinstance(style, dict):
                continue

            ow = Widget.create("obj", lv_obj_t, w_name.type)
            apply = lv_Pvariable(lv_lambda_t, f"lv_theme_apply_{w_name}")
            theme_widget_map[w_name] = apply
            with LambdaContext([(lv_obj_t, "obj")], where=w_name) as context:
                await set_obj_properties(ow, style)
            lv_assign(apply, context.get_lambda())


async def add_top_layer(config):
    if top_conf := config.get(CONF_TOP_LAYER):
        with LocalVariable(
            "top_layer", lv_obj_t, "*", "lv_disp_get_layer_top(lv_component->disp_)"
        ) as top_layer_obj:
            await set_obj_properties(top_layer_obj, top_conf)
            await add_widgets(top_layer_obj, top_conf)
