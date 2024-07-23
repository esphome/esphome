from esphome import automation
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_RANGE_FROM, CONF_RANGE_TO, CONF_STEP, CONF_VALUE

from .codegen import action_to_code, update_to_code
from .defines import (
    CONF_DECIMAL_PLACES,
    CONF_DIGITS,
    CONF_LABEL,
    CONF_ROLLOVER,
    CONF_SPINBOX,
    CONF_TEXTAREA,
)
from .lv_validation import lv_bool, lv_float
from .types import ObjUpdateAction, lv_spinbox_t
from .widget import Widget, WidgetType, get_widget

SPIN_ACTIONS = (
    "INCREMENT",
    "DECREMENT",
    "STEP_NEXT",
    "STEP_PREV",
    "CLEAR",
)

SPINBOX_SCHEMA = {
    cv.Optional(CONF_VALUE): lv_float,
    cv.Optional(CONF_RANGE_FROM, default=0): cv.float_,
    cv.Optional(CONF_RANGE_TO, default=100): cv.float_,
    cv.Optional(CONF_DIGITS, default=4): cv.int_range(1, 10),
    cv.Optional(CONF_STEP, default=1.0): cv.positive_float,
    cv.Optional(CONF_DECIMAL_PLACES, default=0): cv.int_range(0, 6),
    cv.Optional(CONF_ROLLOVER, default=False): lv_bool,
}

SPINBOX_MODIFY_SCHEMA = {
    cv.Required(CONF_VALUE): lv_float,
}


def validate_spinbox(config):
    max_val = 2**31 - 1
    min_val = -1 - max_val
    scale = 10 ** config[CONF_DECIMAL_PLACES]
    range_from = int(config[CONF_RANGE_FROM] * scale)
    range_to = int(config[CONF_RANGE_TO] * scale)
    step = int(config[CONF_STEP] * scale)
    if (
        range_from > max_val
        or range_from < min_val
        or range_to > max_val
        or range_to < min_val
    ):
        raise cv.Invalid("Range outside allowed limits")
    if step <= 0 or step >= (range_to - range_from) / 2:
        raise cv.Invalid("Invalid step value")
    if config[CONF_DIGITS] <= config[CONF_DECIMAL_PLACES]:
        raise cv.Invalid("Number of digits must exceed number of decimal places")
    return config


class SpinboxType(WidgetType):
    def __init__(self):
        super().__init__(CONF_SPINBOX, SPINBOX_SCHEMA, SPINBOX_MODIFY_SCHEMA)

    @property
    def w_type(self):
        return lv_spinbox_t

    async def to_code(self, w: Widget, config):
        init = []
        if CONF_DIGITS in config:
            digits = config[CONF_DIGITS]
            scale = 10 ** config[CONF_DECIMAL_PLACES]
            range_from = int(config[CONF_RANGE_FROM])
            range_to = int(config[CONF_RANGE_TO])
            step = int(config[CONF_STEP])
            w.scale = scale
            w.step = step
            w.range_to = range_to
            w.range_from = range_from
            init.append(
                f"lv_spinbox_set_range({w.obj}, {range_from * scale}, {range_to * scale})"
            )
            init.extend(w.set_property(CONF_STEP, step * scale))
            init.extend(w.set_property(CONF_ROLLOVER, config))
            init.append(
                f"lv_spinbox_set_digit_format({w.obj}, {digits}, {digits - config[CONF_DECIMAL_PLACES]})"
            )
        if value := config.get(CONF_VALUE):
            init.extend(w.set_value(await lv_float.process(value)))
        return init

    def get_uses(self):
        return CONF_TEXTAREA, CONF_LABEL


spinbox_spec = SpinboxType()


@automation.register_action(
    "lvgl.spinbox.increment",
    ObjUpdateAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(lv_spinbox_t),
        },
        key=CONF_ID,
    ),
)
async def spinbox_increment(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = [f"lv_spinbox_increment({widget.obj})"]
    return await action_to_code(init, action_id, widget, template_arg, args)


@automation.register_action(
    "lvgl.spinbox.decrement",
    ObjUpdateAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(lv_spinbox_t),
        },
        key=CONF_ID,
    ),
)
async def spinbox_decrement(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = [f"lv_spinbox_decrement({widget.obj})"]
    return await action_to_code(init, action_id, widget, template_arg, args)


@automation.register_action(
    "lvgl.spinbox.update",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(lv_spinbox_t),
            cv.Required(CONF_VALUE): lv_float,
        }
    ),
)
async def spinbox_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    init = widget.set_value(await lv_float.process(config[CONF_VALUE]))
    return await update_to_code(config, action_id, widget, init, template_arg, args)
