import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

crc8_test_component_ns = cg.esphome_ns.namespace("crc8_test_component")
CRC8TestComponent = crc8_test_component_ns.class_("CRC8TestComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CRC8TestComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
