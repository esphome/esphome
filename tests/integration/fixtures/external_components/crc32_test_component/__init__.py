import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

crc32_test_component_ns = cg.esphome_ns.namespace("crc32_test_component")
CRC32TestComponent = crc32_test_component_ns.class_("CRC32TestComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CRC32TestComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
