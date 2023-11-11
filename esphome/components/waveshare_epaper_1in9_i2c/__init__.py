import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_ID,
    CONF_RESET_PIN,
)

CODEOWNERS = ["nanomad"]
DEPENDENCIES = ["i2c"]

waveshare_epaper_1in9_i2c_ns = cg.esphome_ns.namespace("waveshare_epaper_1in9_i2c")
WaveShareEPaper1in9I2C = waveshare_epaper_1in9_i2c_ns.class_(
    "WaveShareEPaper1in9I2C", cg.PollingComponent
)


CONF_I2C_BUS = "i2c_bus"
CONF_I2C_COMMAND_ADDRESS = "i2c_command_address"
CONF_I2C_DATA_ADDRESS = "i2c_data_address"

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(WaveShareEPaper1in9I2C),
        cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_I2C_BUS): cv.use_id(i2c.I2CBus),
        cv.Required(CONF_I2C_COMMAND_ADDRESS): cv.int_range(min=0, max=0xFF),
        cv.Required(CONF_I2C_DATA_ADDRESS): cv.int_range(min=0, max=0xFF),
    }
).extend(cv.polling_component_schema("180s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_ping(reset_pin))
    busy_pin = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
    cg.add(var.set_busy_pin(busy_pin))

    i2c_bus = await cg.get_variable(config[CONF_I2C_BUS])

    command_address = config[CONF_I2C_COMMAND_ADDRESS]
    cg.add(var.create_command_device(i2c_bus, command_address))

    data_address = config[CONF_I2C_DATA_ADDRESS]
    cg.add(var.create_data_device(i2c_bus, data_address))
