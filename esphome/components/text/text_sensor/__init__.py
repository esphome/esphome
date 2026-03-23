import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SOURCE_ID

from .. import Text, text_ns

CODEOWNERS = ["@clydebarrow"]

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
    var = cg.new_Pvariable(config[CONF_ID], source)
    await text_sensor.register_text_sensor(var, config)
    await cg.register_component(var, config)
