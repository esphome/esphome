import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_SOURCE_ID

from .. import Text, text_ns

TextTextSensor = text_ns.class_("TextTextSensor", text_sensor.TextSensor, cg.Component)


CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(TextTextSensor)
    .extend(
        {
            cv.Required(CONF_SOURCE_ID): cv.use_id(Text),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    source = await cg.get_variable(config[CONF_SOURCE_ID])
    var = await text_sensor.new_text_sensor(config, source)
    await cg.register_component(var, config)
