import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@pebblebed-tech"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_EE_MEMORY_TYPE = "ee_memory_type"
CONF_I2C_BUFFER_SIZE = "i2c_buffer_size"

ext_eeprom_component_ns = cg.esphome_ns.namespace("external_eeprom")
ExtEepromComponent = ext_eeprom_component_ns.class_(
    "ExtEepromComponent", cg.Component, i2c.I2CDevice
)

EEPROM_TYPES = {
    "24XX00": ext_eeprom_component_ns.EEE_24XX00,
    "24XX01": ext_eeprom_component_ns.EEE_24XX01,
    "24XX02": ext_eeprom_component_ns.EEE_24XX02,
    "24XX04": ext_eeprom_component_ns.EEE_24XX04,
    "24XX08": ext_eeprom_component_ns.EEE_24XX08,
    "24XX16": ext_eeprom_component_ns.EEE_24XX16,
    "24XX32": ext_eeprom_component_ns.EEE_24XX32,
    "24XX64": ext_eeprom_component_ns.EEE_24XX64,
    "24XX128": ext_eeprom_component_ns.EEE_24XX128,
    "24XX256": ext_eeprom_component_ns.EEE_24XX256,
    "24XX512": ext_eeprom_component_ns.EEE_24XX512,
    "24XX1025": ext_eeprom_component_ns.EEE_24XX1025,
    "24XX2048": ext_eeprom_component_ns.EEE_24XX2048,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ExtEepromComponent),
            cv.Required(CONF_EE_MEMORY_TYPE): cv.one_of(*EEPROM_TYPES, upper=True),
            cv.Required(CONF_I2C_BUFFER_SIZE): cv.uint8_t,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x57))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_i2c_buffer_size(config[CONF_I2C_BUFFER_SIZE]))
    cg.add(var.set_memory_type(EEPROM_TYPES[config[CONF_EE_MEMORY_TYPE]]))
