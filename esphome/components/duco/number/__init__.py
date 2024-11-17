import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

UNIT_DAYS = "days"

CONF_FILTER_REMAINING = "filter_remaining"

duco_ns = cg.esphome_ns.namespace("duco")
DucoComfortTemperature = duco_ns.class_(
    "DucoComfortTemperature", cg.PollingComponent, number.Number
)

CONFIG_SCHEMA = (
    number.number_schema(
        DucoComfortTemperature,
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
    )
    .extend(DUCO_COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await number.register_number(var, config, min_value=10.0, max_value=25.5, step=0.1)

    parent = await cg.get_variable(config[CONF_DUCO_ID])
    cg.add(var.set_parent(parent))
