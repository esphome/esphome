import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE

from .. import ROJAFLEX_DEVICE_SCHEMA, RojaflexDevice, register_rojaflex_device, rojaflex_ns

DEPENDENCIES = ["rojaflex"]

NUMBER_TYPES = {
    "tx_repetitions": "TX_REPETITIONS",
}

RojaflexNumber = rojaflex_ns.class_("RojaflexNumber", number.Number, cg.Component, RojaflexDevice)
RojaflexNumberType = rojaflex_ns.enum("RojaflexNumberType", is_class=True)

CONFIG_SCHEMA = (
    number.number_schema(RojaflexNumber)
    .extend(ROJAFLEX_DEVICE_SCHEMA)
    .extend({cv.Required(CONF_TYPE): cv.enum(NUMBER_TYPES, lower=True)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await number.register_number(var, config, min_value=1, max_value=9, step=1)
    await cg.register_component(var, config)
    await register_rojaflex_device(var, config)
    cg.add(var.set_number_type(getattr(RojaflexNumberType, NUMBER_TYPES[config[CONF_TYPE]])))
