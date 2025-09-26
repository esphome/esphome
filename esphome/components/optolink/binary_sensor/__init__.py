import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID

from .. import CONF_OPTOLINK_ID, SENSOR_BASE_SCHEMA, optolink_ns

DEPENDENCIES = ["optolink"]
CODEOWNERS = ["@j0ta29"]


OptolinkBinarySensor = optolink_ns.class_(
    "OptolinkBinarySensor", binary_sensor.BinarySensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(OptolinkBinarySensor)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(OptolinkBinarySensor),
            cv.Required(CONF_ADDRESS): cv.hex_uint32_t,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(SENSOR_BASE_SCHEMA)
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_OPTOLINK_ID])
    var = cg.new_Pvariable(config[CONF_ID], component)

    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    cg.add(var.set_address(config[CONF_ADDRESS]))
