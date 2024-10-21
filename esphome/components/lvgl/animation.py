from esphome import codegen as cg, config_validation as cv
from esphome.components.lvgl.defines import CONF_WIDGETS, CONF_ANIMATIONS, LValidator
from esphome.components.lvgl.lvcode import LambdaContext
from esphome.components.lvgl.schemas import STYLE_PROPS
from esphome.components.lvgl.types import LvAnimation, lv_obj_t
from esphome.const import CONF_DURATION, CONF_ID, CONF_FROM, CONF_TO
from esphome.cpp_generator import TemplateArguments

CONF_START_DELAY = "start_delay"

def from_to(validator):
    return cv.Schema(
        {
            cv.Required(CONF_FROM): validator,
            cv.Required(CONF_TO): validator,
        }
    )


ANIMABLE_STYLES = {
    k: from_to(v) for k, v in STYLE_PROPS.items() if isinstance(v, LValidator) and v.animatable
}

ANIMATION_CONFIG = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(LvAnimation),
        cv.Optional(CONF_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_START_DELAY): cv.positive_time_period_milliseconds,
    }
)
ANIMATION_SCHEMA = ANIMATION_CONFIG.extend(
    {
        cv.Required(CONF_WIDGETS): cv.ensure_list(
            cv.Schema({
                cv.Required(CONF_ID): cv.use_id(lv_obj_t),
            }).extend({
                cv.Optional(k): v for k, v in ANIMABLE_STYLES.items()
            })
        )
    }
)

print(ANIMABLE_STYLES.keys())

async def animations_to_code(config):
    for animation in config.get(CONF_ANIMATIONS, []):
        widgets = animation[CONF_WIDGETS]
        print(widgets)
        widget_count = len(animation[CONF_WIDGETS])
        property_count = sum([len([k for k in widget if k in ANIMABLE_STYLES.keys()]) for widget in widgets])
        print(widget_count, property_count)
        var = cg.new_Pvariable(animation[CONF_ID], TemplateArguments(property_count))
        #widget_properties = {k: literal for i, (k, v) in enumerate(widgets.items())}
        #with LambdaContext([([cg.uint32], "values")]) as ctx:
