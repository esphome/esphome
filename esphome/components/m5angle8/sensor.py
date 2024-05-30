import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor

from esphome.const import (
    CONF_CHANNEL,
    ICON_ROTATE_RIGHT,
    STATE_CLASS_MEASUREMENT,
)

from . import M5Angle8Component, m5angle8_ns, CONF_M5STACK_8ANGLE_ID


M5Angle8SensorKnob = m5angle8_ns.class_(
    "M5Angle8SensorKnob",
    sensor.Sensor,
    cg.PollingComponent,
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5Angle8SensorKnob),
            cv.GenerateID(CONF_M5STACK_8ANGLE_ID): cv.use_id(M5Angle8Component),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
        }
    )
    .extend(
        sensor.sensor_schema(
            M5Angle8SensorKnob,
            accuracy_decimals=2,
            icon=ICON_ROTATE_RIGHT,
            state_class=STATE_CLASS_MEASUREMENT,
        )
    )
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_M5STACK_8ANGLE_ID])
    sens = await sensor.new_sensor(config)
    cg.add(sens.set_parent(hub, config[CONF_CHANNEL]))
    await cg.register_component(sens, config)
