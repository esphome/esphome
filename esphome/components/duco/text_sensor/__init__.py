import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

duco_ns = cg.esphome_ns.namespace("duco")
DucoSerial = duco_ns.class_("DucoSerial", cg.PollingComponent, text_sensor.TextSensor)

CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema(DucoSerial)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("300s"))
    .extend(DUCO_COMPONENT_SCHEMA)
    .extend({cv.GenerateID(): cv.declare_id(DucoSerial)})
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await text_sensor.register_text_sensor(var, config)

    parent = await cg.get_variable(config[CONF_DUCO_ID])
    cg.add(parent.add_sensor_item(var))
