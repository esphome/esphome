import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_SPEED,
    STATE_CLASS_MEASUREMENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
)

from .. import CONF_EMC2303_ID, CONF_FAN, Emc2303Component, emc2303_ns

DEPENDENCIES = ["emc2303"]

Emc2303Sensor = emc2303_ns.class_("Emc2303Sensor", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(Emc2303Sensor),
        cv.GenerateID(CONF_EMC2303_ID): cv.use_id(Emc2303Component),
        cv.Required(CONF_FAN): cv.int_range(min=1, max=3),
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:fan",
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    paren = await cg.get_variable(config[CONF_EMC2303_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren)
    cg.add(var.set_fan(config[CONF_FAN]))
    await cg.register_component(var, config)

    if CONF_SPEED in config:
        sens = await sensor.new_sensor(config[CONF_SPEED])
        cg.add(var.set_speed_sensor(sens))
