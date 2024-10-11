import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_SPEED,
    DEVICE_CLASS_SPEED,
    STATE_CLASS_MEASUREMENT,
    UNIT_KILOMETER_PER_HOUR,
)
from .. import ld2415h_ns, LD2415HComponent, CONF_LD2415H_ID

LD2415HSensor = ld2415h_ns.class_("LD2415HSensor", sensor.Sensor, cg.Component)

ICON_SPEEDOMETER = "mdi:speedometer"

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(LD2415HSensor),
            cv.GenerateID(CONF_LD2415H_ID): cv.use_id(LD2415HComponent),
            cv.Optional(CONF_SPEED): sensor.sensor_schema(
                device_class=DEVICE_CLASS_SPEED,
                state_class=STATE_CLASS_MEASUREMENT,
                unit_of_measurement=UNIT_KILOMETER_PER_HOUR,
                icon=ICON_SPEEDOMETER,
                accuracy_decimals=1,
            ),
        }
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if speed := config.get(CONF_SPEED):
        sens = await sensor.new_sensor(speed)
        cg.add(var.set_speed_sensor(sens))
    ld2415h = await cg.get_variable(config[CONF_LD2415H_ID])
    cg.add(ld2415h.register_listener(var))
