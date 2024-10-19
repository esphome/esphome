from esphome import codegen as cg, config_validation as cv
from esphome.components.lvgl.defines import CONF_WIDGETS
from esphome.components.lvgl.schemas import STYLE_PROPS
from esphome.const import CONF_DURATION, CONF_ID

CONF_START_DELAY = "start_delay"

ANIMABLE_STYLES = {
    k: v for k, v in STYLE_PROPS.items() if v.rtype in [cg.int_, cg.uint32]
}

ANIMATION_CONFIG = cv.Schema(
    {
        cv.Required(CONF_ID): cv.GenerateID,
        cv.Optional(CONF_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_START_DELAY): cv.positive_time_period_milliseconds,
    }
)
ANIMATION_SCHEMA = ANIMATION_CONFIG.extend(
    {
        cv.Required(CONF_WIDGETS): cv.ensure_list(
            cv.Schema({cv.Required(CONF_ID): cv.use_id})
        ),
    }
)
