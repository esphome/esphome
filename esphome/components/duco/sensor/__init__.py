import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

duco_ns = cg.esphome_ns.namespace("duco")
DucoSensor = duco_ns.class_("DucoSensor", cg.PollingComponent, sensor.Sensor)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(DucoSensor)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
    .extend(DUCO_COMPONENT_SCHEMA)
    .extend({cv.GenerateID(): cv.declare_id(DucoSensor)})
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await sensor.register_sensor(var, config)

    parent = await cg.get_variable(config[CONF_DUCO_ID])
    cg.add(parent.add_sensor_item(var))
