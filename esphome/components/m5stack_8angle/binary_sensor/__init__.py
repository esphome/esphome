import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from .. import CONF_M5STACK_8ANGLE_ID, M5Stack8AngleComponent, m5stack_8angle_ns

M5Stack8AngleSwitchBinarySensor = m5stack_8angle_ns.class_(
    "M5Stack8AngleSwitchBinarySensor",
    binary_sensor.BinarySensor,
    cg.PollingComponent,
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_M5STACK_8ANGLE_ID): cv.use_id(M5Stack8AngleComponent),
        }
    )
    .extend(binary_sensor.binary_sensor_schema(M5Stack8AngleSwitchBinarySensor))
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_M5STACK_8ANGLE_ID])
    sens = await binary_sensor.new_binary_sensor(config)
    cg.add(sens.set_parent(hub))
    await cg.register_component(sens, config)
