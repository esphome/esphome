import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_AMBER_API_ID, AmberApiComponent, amber_api_ns

DEPENDENCIES = ["amber_api"]

AmberApiDescriptorTextSensor = amber_api_ns.class_(
    "AmberApiDescriptorTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(AmberApiDescriptorTextSensor)
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
    await text_sensor.register_text_sensor(var, config)

    parent = await cg.get_variable(config[CONF_AMBER_API_ID])
    cg.add(parent.register_listener(var))
