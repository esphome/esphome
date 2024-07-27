import esphome.codegen as cg
from esphome.components.binary_sensor import BinarySensor
from esphome.components.rotary_encoder.sensor import RotaryEncoderSensor
import esphome.config_validation as cv
from esphome.const import CONF_GROUP, CONF_ID, CONF_SENSOR

from .defines import (
    CONF_ENTER_BUTTON,
    CONF_LEFT_BUTTON,
    CONF_LONG_PRESS_REPEAT_TIME,
    CONF_LONG_PRESS_TIME,
    CONF_RIGHT_BUTTON,
    CONF_ROTARY_ENCODERS,
)
from .helpers import lvgl_components_required
from .lvcode import add_group, lv, lv_add, lv_expr
from .schemas import ENCODER_SCHEMA
from .types import lv_indev_type_t

ROTARY_ENCODER_CONFIG = cv.ensure_list(
    ENCODER_SCHEMA.extend(
        {
            cv.Required(CONF_ENTER_BUTTON): cv.use_id(BinarySensor),
            cv.Required(CONF_SENSOR): cv.Any(
                cv.use_id(RotaryEncoderSensor),
                cv.Schema(
                    {
                        cv.Optional(CONF_LEFT_BUTTON): cv.use_id(BinarySensor),
                        cv.Optional(CONF_RIGHT_BUTTON): cv.use_id(BinarySensor),
                    }
                ),
            ),
        }
    )
)


async def rotary_encoders_to_code(var, config):
    for enc_conf in config.get(CONF_ROTARY_ENCODERS, ()):
        lvgl_components_required.add("KEY_LISTENER")
        lvgl_components_required.add("ROTARY_ENCODER")
        lpt = enc_conf[CONF_LONG_PRESS_TIME].total_milliseconds & 0xFFFF
        lprt = enc_conf[CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds & 0xFFFF
        listener = cg.new_Pvariable(
            enc_conf[CONF_ID], lv_indev_type_t.LV_INDEV_TYPE_ENCODER, lpt, lprt
        )
        await cg.register_parented(listener, var)
        if sensor := enc_conf.get(CONF_SENSOR):
            if isinstance(sensor, dict):
                b_sensor = await cg.get_variable(sensor[CONF_LEFT_BUTTON])
                lv_add(listener.set_left_button(b_sensor))
                b_sensor = await cg.get_variable(sensor[CONF_RIGHT_BUTTON])
                lv_add(listener.set_right_button(b_sensor))
            else:
                sensor = await cg.get_variable(sensor)
                lv_add(listener.set_sensor(sensor))
        b_sensor = await cg.get_variable(enc_conf[CONF_ENTER_BUTTON])
        lv_add(listener.set_enter_button(b_sensor))
        if group := add_group(enc_conf.get(CONF_GROUP)):
            lv.indev_set_group(lv_expr.indev_drv_register(listener.get_drv()), group)
        else:
            lv.indev_drv_register(listener.get_drv())
