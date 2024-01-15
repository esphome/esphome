import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@pebblebed-tech"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_EE_MEMORY_SIZE = "ee_memory_size"
CONF_EE_PAGE_SIZE = "ee_page_size"
CONF_EE_PAGE_WRITE_TIME = "ee_page_write_time"
CONF_I2C_BUFFER_SIZE = "i2c_buffer_size"

ext_eeprom_component_ns = cg.esphome_ns.namespace("external_eeprom")
ExtEepromComponent = ext_eeprom_component_ns.class_(
    "ExtEepromComponent", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ExtEepromComponent),
            cv.Required(CONF_EE_MEMORY_SIZE): cv.uint32_t,
            cv.Required(CONF_EE_PAGE_SIZE): cv.uint16_t,
            cv.Required(CONF_EE_PAGE_WRITE_TIME): cv.uint8_t,
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
    cg.add(var.set_memory_size(config[CONF_EE_MEMORY_SIZE]))
    cg.add(var.set_page_size(config[CONF_EE_PAGE_SIZE]))
    cg.add(var.set_page_write_time(config[CONF_EE_PAGE_WRITE_TIME]))
    cg.add(var.set_i2c_buffer_size(config[CONF_I2C_BUFFER_SIZE]))
