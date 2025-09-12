import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISTANCE,
    CONF_ID,
    CONF_SPEED,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SPEED,
    ICON_RULER,
    UNIT_METER,
)

from .. import CONF_C4001_ID, c4001Component, dfrobot_c4001_ns

C4001Sensor = dfrobot_c4001_ns.class_("C4001Sensor", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(C4001Sensor),
        cv.Required(CONF_C4001_ID): cv.use_id(c4001Component),
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            device_class=DEVICE_CLASS_SPEED,
            unit_of_measurement="m/s",
            icon="mdi:speedometer",
            accuracy_decimals=1,
        ),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_METER,
            icon=ICON_RULER,
            accuracy_decimals=1,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    c4001_sensor = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(c4001_sensor, config)
    if CONF_SPEED in config:
        sens_conf = config[CONF_SPEED]
        sens = await sensor.new_sensor(sens_conf)
        cg.add(c4001_sensor.set_speed_sensor(sens))
    if CONF_DISTANCE in config:
        sens_conf = config[CONF_DISTANCE]
        sens = await sensor.new_sensor(sens_conf)
        cg.add(c4001_sensor.set_distance_sensor(sens))
    c4001_component = await cg.get_variable(config[CONF_C4001_ID])
    cg.add(c4001_component.register_listener(c4001_sensor))
