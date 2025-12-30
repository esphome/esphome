import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_AMBER_API_ID, AmberApiComponent, amber_api_ns

DEPENDENCIES = ["amber_api"]

AmberApiSpikeBinarySensor = amber_api_ns.class_(
    "AmberApiSpikeBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(AmberApiSpikeBinarySensor)
    .extend(
        {
            cv.GenerateID(CONF_AMBER_API_ID): cv.use_id(AmberApiComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    parent = await cg.get_variable(config[CONF_AMBER_API_ID])
    cg.add(parent.register_listener(var))
