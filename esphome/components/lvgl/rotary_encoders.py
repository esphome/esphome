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
from .lvcode import lv, lv_add, lv_expr
from .schemas import ENCODER_SCHEMA
from .types import lv_indev_type_t
from .widget import add_group

ROTARY_ENCODER_CONFIG = cv.ensure_list(
    ENCODER_SCHEMA.extend(
        {
            cv.Required(CONF_ENTER_BUTTON): cv.use_id(BinarySensor),
            cv.Required(CONF_SENSOR): cv.Any(
                cv.use_id(RotaryEncoderSensor),
                cv.Schema(
                    {
                        cv.Required(CONF_LEFT_BUTTON): cv.use_id(BinarySensor),
                        cv.Required(CONF_RIGHT_BUTTON): cv.use_id(BinarySensor),
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
        lpt = enc_conf[CONF_LONG_PRESS_TIME].total_milliseconds
        lprt = enc_conf[CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds
        listener = cg.new_Pvariable(
            enc_conf[CONF_ID], lv_indev_type_t.LV_INDEV_TYPE_ENCODER, lpt, lprt
        )
        await cg.register_parented(listener, var)
        if sensor_config := enc_conf.get(CONF_SENSOR):
            if isinstance(sensor_config, dict):
                b_sensor = await cg.get_variable(sensor_config[CONF_LEFT_BUTTON])
                cg.add(listener.set_left_button(b_sensor))
                b_sensor = await cg.get_variable(sensor_config[CONF_RIGHT_BUTTON])
                cg.add(listener.set_right_button(b_sensor))
            else:
                sensor_config = await cg.get_variable(sensor_config)
                lv_add(listener.set_sensor(sensor_config))
        b_sensor = await cg.get_variable(enc_conf[CONF_ENTER_BUTTON])
        cg.add(listener.set_enter_button(b_sensor))
        if group := add_group(enc_conf.get(CONF_GROUP)):
            lv.indev_set_group(lv_expr.indev_drv_register(listener.get_drv()), group)
        else:
            lv.indev_drv_register(listener.get_drv())
