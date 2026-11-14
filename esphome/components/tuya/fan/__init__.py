import esphome.codegen as cg
from esphome.components import fan
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_OPTIMISTIC,
    CONF_SPEED_COUNT,
    CONF_SWITCH_DATAPOINT,
)

from .. import CONF_TUYA_ID, Tuya, register_tuya_child, tuya_ns

DEPENDENCIES = ["tuya"]

CONF_SPEED_DATAPOINT = "speed_datapoint"
CONF_OSCILLATION_DATAPOINT = "oscillation_datapoint"
CONF_DIRECTION_DATAPOINT = "direction_datapoint"

TuyaFan = tuya_ns.class_("TuyaFan", fan.Fan)

CONFIG_SCHEMA = cv.All(
    fan.fan_schema(TuyaFan).extend(
        {
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Optional(CONF_OSCILLATION_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_SPEED_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_SWITCH_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_DIRECTION_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_SPEED_COUNT, default=3): cv.int_range(min=1, max=256),
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
        }
    ),
    cv.has_at_least_one_key(CONF_SPEED_DATAPOINT, CONF_SWITCH_DATAPOINT),
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_TUYA_ID])

    var = cg.new_Pvariable(config[CONF_ID], hub, config[CONF_SPEED_COUNT])
    await fan.register_fan(var, config)

    cg.add_optional(var.set_speed_id, config.get(CONF_SPEED_DATAPOINT))
    cg.add_optional(var.set_switch_id, config.get(CONF_SWITCH_DATAPOINT))
    cg.add_optional(var.set_oscillation_id, config.get(CONF_OSCILLATION_DATAPOINT))
    cg.add_optional(var.set_direction_id, config.get(CONF_DIRECTION_DATAPOINT))
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    register_tuya_child(hub, var)
