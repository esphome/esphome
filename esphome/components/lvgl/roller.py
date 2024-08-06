import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_MODE, CONF_OPTIONS

from .defines import (
    CONF_ANIMATED,
    CONF_MAIN,
    CONF_SELECTED,
    CONF_SELECTED_INDEX,
    CONF_VISIBLE_ROW_COUNT,
    ROLLER_MODES,
    literal,
)
from .label import CONF_LABEL
from .lv_validation import animated, lv_int, option_string
from .lvcode import lv
from .types import LvSelect
from .widget import WidgetType

CONF_ROLLER = "roller"
lv_roller_t = LvSelect("lv_roller_t")

ROLLER_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SELECTED_INDEX): cv.templatable(cv.int_),
        cv.Optional(CONF_VISIBLE_ROW_COUNT): lv_int,
    }
)

ROLLER_SCHEMA = ROLLER_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_OPTIONS): cv.ensure_list(option_string),
        cv.Optional(CONF_MODE, default="NORMAL"): ROLLER_MODES.one_of,
    }
)

ROLLER_MODIFY_SCHEMA = ROLLER_BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_ANIMATED, default=True): animated,
    }
)


class RollerType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_ROLLER,
            lv_roller_t,
            (CONF_MAIN, CONF_SELECTED),
            ROLLER_SCHEMA,
            ROLLER_MODIFY_SCHEMA,
        )

    async def to_code(self, w, config):
        if options := config.get(CONF_OPTIONS):
            mode = await ROLLER_MODES.process(config[CONF_MODE])
            text = cg.safe_exp("\n".join(options))
            lv.roller_set_options(w.obj, text, mode)
        animopt = literal(config.get(CONF_ANIMATED) or "LV_ANIM_OFF")
        if CONF_SELECTED_INDEX in config:
            if selected := config[CONF_SELECTED_INDEX]:
                value = await lv_int.process(selected)
                lv.roller_set_selected(w.obj, value, animopt)
        await w.set_property(
            CONF_VISIBLE_ROW_COUNT,
            await lv_int.process(config.get(CONF_VISIBLE_ROW_COUNT)),
        )

    @property
    def animated(self):
        return True

    def get_uses(self):
        return (CONF_LABEL,)


roller_spec = RollerType()
