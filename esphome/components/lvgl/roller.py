import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_MODE, CONF_OPTIONS

from .defines import (
    CONF_ANIMATED,
    CONF_LABEL,
    CONF_ROLLER,
    CONF_SELECTED_INDEX,
    CONF_VISIBLE_ROW_COUNT,
    ROLLER_MODES,
)
from .lv_validation import animated, lv_int, option_string
from .types import lv_roller_t
from .widget import WidgetType

ROLLER_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SELECTED_INDEX): cv.templatable(cv.int_),
        cv.Optional(CONF_VISIBLE_ROW_COUNT): lv_int,
        cv.Optional(CONF_MODE, default="NORMAL"): ROLLER_MODES.one_of,
    }
)

ROLLER_SCHEMA = ROLLER_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_OPTIONS): cv.ensure_list(option_string),
    }
)

ROLLER_MODIFY_SCHEMA = ROLLER_BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_ANIMATED, default=True): animated,
    }
)


class RollerType(WidgetType):
    def __init__(self):
        super().__init__(CONF_ROLLER, ROLLER_SCHEMA, ROLLER_MODIFY_SCHEMA)

    @property
    def w_type(self):
        return lv_roller_t

    async def to_code(self, w, config):
        init = []
        mode = config[CONF_MODE]
        if options := config.get(CONF_OPTIONS):
            text = cg.safe_exp("\n".join(options))
            init.append(f"lv_roller_set_options({w.obj}, {text}, {mode})")
        animopt = config.get(CONF_ANIMATED) or "LV_ANIM_OFF"
        if selected := config.get(CONF_SELECTED_INDEX):
            value = await lv_int.process(selected)
            init.extend(w.set_property("selected", value, animopt))
        init.extend(
            w.set_property(
                CONF_VISIBLE_ROW_COUNT,
                await lv_int.process(config.get(CONF_VISIBLE_ROW_COUNT)),
            )
        )
        return init

    @property
    def animated(self):
        return True

    def get_uses(self):
        return (CONF_LABEL,)


roller_spec = RollerType()
