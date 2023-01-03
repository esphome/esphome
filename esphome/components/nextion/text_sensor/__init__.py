from esphome.components import text_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID

from .. import nextion_ns, CONF_NEXTION_ID

from ..base_component import (
    setup_component_core_,
    CONFIG_TEXT_COMPONENT_SCHEMA,
)

CODEOWNERS = ["@senexcrenshaw"]

NextionTextSensor = nextion_ns.class_(
    "NextionTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(NextionTextSensor)
    .extend(CONFIG_TEXT_COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("never"))
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NEXTION_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub)
    await text_sensor.register_text_sensor(var, config)
    await cg.register_component(var, config)

    cg.add(hub.register_textsensor_component(var))

    await setup_component_core_(var, config, ".txt")
