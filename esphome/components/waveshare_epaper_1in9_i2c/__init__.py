import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_I2C_ID,
    CONF_ID,
    CONF_RESET_PIN,
)

CODEOWNERS = ["@nanomad"]
DEPENDENCIES = ["i2c"]

waveshare_epaper_1in9_i2c_ns = cg.esphome_ns.namespace("waveshare_epaper_1in9_i2c")
WaveShareEPaper1in9I2C = waveshare_epaper_1in9_i2c_ns.class_(
    "WaveShareEPaper1in9I2C", cg.PollingComponent
)


CONF_COMMAND_ADDRESS = "command_address"
CONF_DATA_ADDRESS = "data_address"
MULTI_CONF = True

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(WaveShareEPaper1in9I2C),
        cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
        cv.GenerateID(CONF_I2C_ID): cv.use_id(i2c.I2CBus),
        cv.Optional(CONF_COMMAND_ADDRESS, default=0x3C): cv.int_range(min=0, max=0xFF),
        cv.Optional(CONF_DATA_ADDRESS, default=0x3D): cv.int_range(min=0, max=0xFF),
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset_pin))
    busy_pin = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
    cg.add(var.set_busy_pin(busy_pin))

    i2c_bus = await cg.get_variable(config[CONF_I2C_ID])

    command_address = config[CONF_COMMAND_ADDRESS]
    cg.add(var.create_command_device(i2c_bus, command_address))

    data_address = config[CONF_DATA_ADDRESS]
    cg.add(var.create_data_device(i2c_bus, data_address))
