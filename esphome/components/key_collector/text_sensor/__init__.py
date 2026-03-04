import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.text_sensor import TextSensor
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import literal
from esphome.types import TemplateArgsType

from .. import CONF_ON_RESULT, CONF_SOURCE_ID, TRIGGER_TYPES, KeyCollector

CONFIG_SCHEMA = text_sensor.text_sensor_schema(TextSensor).extend(
    {
        cv.GenerateID(CONF_SOURCE_ID): cv.use_id(KeyCollector),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SOURCE_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await text_sensor.register_text_sensor(var, config)
    args = TRIGGER_TYPES[CONF_ON_RESULT]
    arglist: TemplateArgsType = [(arg.type, arg.name) for arg in args]
    cg.add(
        parent.add_on_result_callback(
            await cg.process_lambda(var.publish_state(literal(args[0].name)), arglist)
        )
    )
