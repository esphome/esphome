from esphome import automation, codegen as cg
from esphome.components.binary_sensor import BinarySensor
import esphome.config_validation as cv
from esphome.const import CONF_GROUP, CONF_ID

from .defines import (
    CONF_INITIAL_FOCUS,
    CONF_KEYPADS,
    CONF_LONG_PRESS_REPEAT_TIME,
    CONF_LONG_PRESS_TIME,
    add_lv_use,
    literal,
)
from .encoders import set_group_to_code
from .lvcode import lv, lv_assign, lv_expr, lv_Pvariable
from .schemas import ENCODER_SCHEMA, SET_GROUP_ACTION_SCHEMA
from .types import LvglAction, lv_group_t, lv_indev_type_t
from .widgets import get_widgets

KEYPAD_KEYS = (
    "up",
    "down",
    "right",
    "left",
    "esc",
    "del",
    "backspace",
    "enter",
    "next",
    "prev",
    "home",
    "end",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "#",
    "*",
)

KEYPADS_CONFIG = cv.ensure_list(
    ENCODER_SCHEMA.extend(
        {cv.Optional(key): cv.use_id(BinarySensor) for key in KEYPAD_KEYS}
    )
)


async def keypads_to_code(var, config, default_group):
    for enc_conf in config[CONF_KEYPADS]:
        add_lv_use("KEY_LISTENER")
        lpt = enc_conf[CONF_LONG_PRESS_TIME].total_milliseconds
        lprt = enc_conf[CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds
        listener = cg.new_Pvariable(
            enc_conf[CONF_ID], lv_indev_type_t.LV_INDEV_TYPE_KEYPAD, lpt, lprt
        )
        await cg.register_parented(listener, var)
        for key in [x for x in enc_conf if x in KEYPAD_KEYS]:
            b_sensor = await cg.get_variable(enc_conf[key])
            cg.add(listener.add_button(b_sensor, literal(f"LV_KEY_{key.upper()}")))

        if group := enc_conf.get(CONF_GROUP):
            group = lv_Pvariable(lv_group_t, group)
            lv_assign(group, lv_expr.group_create())
        else:
            group = default_group
        lv.indev_set_group(listener.get_drv(), group)


async def keypad_initial_focus_to_code(config):
    for keyp_conf in config[CONF_KEYPADS]:
        if default_focus := keyp_conf.get(CONF_INITIAL_FOCUS):
            widget = await get_widgets(default_focus)
            lv.group_focus_obj(widget[0].obj)


automation.register_action(
    "lvgl.keypad.set_group", LvglAction, SET_GROUP_ACTION_SCHEMA
)(set_group_to_code)
