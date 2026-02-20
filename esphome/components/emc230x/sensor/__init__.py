import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_SPEED,
    STATE_CLASS_MEASUREMENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
)

from .. import CONF_EMC230X_ID, CONF_FAN, Emc230xComponent, emc230x_ns

DEPENDENCIES = ["emc230x"]

Emc230xSensor = emc230x_ns.class_("Emc230xSensor", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(Emc230xSensor),
        cv.GenerateID(CONF_EMC230X_ID): cv.use_id(Emc230xComponent),
        cv.Required(CONF_FAN): cv.int_range(min=1, max=5),
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:fan",
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    paren = await cg.get_variable(config[CONF_EMC230X_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren)
    cg.add(var.set_fan(config[CONF_FAN]))
    await cg.register_component(var, config)

    if CONF_SPEED in config:
        sens = await sensor.new_sensor(config[CONF_SPEED])
        cg.add(var.set_speed_sensor(sens))
