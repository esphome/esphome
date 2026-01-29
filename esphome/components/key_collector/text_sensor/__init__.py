import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_SOURCE_ID, KeyCollector, key_collector_ns

KeyCollectorTextSensor = key_collector_ns.class_(
    "KeyCollectorTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(KeyCollectorTextSensor).extend(
    {
        cv.GenerateID(CONF_SOURCE_ID): cv.use_id(KeyCollector),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SOURCE_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await text_sensor.register_text_sensor(var, config)
    await cg.register_component(var, config)
