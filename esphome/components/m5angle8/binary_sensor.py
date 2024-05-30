import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import M5Angle8Component, m5angle8_ns, CONF_M5STACK_8ANGLE_ID


M5Angle8SensorSwitch = m5angle8_ns.class_(
    "M5Angle8SensorSwitch",
    binary_sensor.BinarySensor,
    cg.PollingComponent,
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_M5STACK_8ANGLE_ID): cv.use_id(M5Angle8Component),
        }
    )
    .extend(binary_sensor.binary_sensor_schema(M5Angle8SensorSwitch))
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_M5STACK_8ANGLE_ID])
    sens = await binary_sensor.new_binary_sensor(config)
    cg.add(sens.set_parent(hub))
    await cg.register_component(sens, config)
