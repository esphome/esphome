import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_FREQUENCY,
    CONF_ID,
    CONF_INPUT,
    CONF_OUTPUT,
    CONF_SCAN,
    CONF_SCL,
    CONF_SDA,
    CONF_ADDRESS,
    CONF_I2C_ID,
)
from esphome.core import coroutine_with_priority, CORE

CODEOWNERS = ["@esphome/core"]
i2c_ns = cg.esphome_ns.namespace("i2c")
I2CBus = i2c_ns.class_("I2CBus")
ArduinoI2CBus = i2c_ns.class_("ArduinoI2CBus", I2CBus, cg.Component)
IDFI2CBus = i2c_ns.class_("IDFI2CBus", I2CBus, cg.Component)
I2CDevice = i2c_ns.class_("I2CDevice")


CONF_SDA_PULLUP_ENABLED = "sda_pullup_enabled"
CONF_SCL_PULLUP_ENABLED = "scl_pullup_enabled"
MULTI_CONF = True


def _bus_declare_type(value):
    if CORE.using_arduino:
        return cv.declare_id(ArduinoI2CBus)(value)
    if CORE.using_esp_idf:
        return cv.declare_id(IDFI2CBus)(value)
    raise NotImplementedError


pin_with_input_and_output_support = cv.All(
    pins.internal_gpio_pin_number({CONF_INPUT: True}),
    pins.internal_gpio_pin_number({CONF_OUTPUT: True}),
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): _bus_declare_type,
        cv.Optional(CONF_SDA, default="SDA"): pin_with_input_and_output_support,
        cv.SplitDefault(CONF_SDA_PULLUP_ENABLED, esp32_idf=True): cv.All(
            cv.only_with_esp_idf, cv.boolean
        ),
        cv.Optional(CONF_SCL, default="SCL"): pin_with_input_and_output_support,
        cv.SplitDefault(CONF_SCL_PULLUP_ENABLED, esp32_idf=True): cv.All(
            cv.only_with_esp_idf, cv.boolean
        ),
        cv.Optional(CONF_FREQUENCY, default="50kHz"): cv.All(
            cv.frequency, cv.Range(min=0, min_included=False)
        ),
        cv.Optional(CONF_SCAN, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(1.0)
async def to_code(config):
    cg.add_global(i2c_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_sda_pin(config[CONF_SDA]))
    if CONF_SDA_PULLUP_ENABLED in config:
        cg.add(var.set_sda_pullup_enabled(config[CONF_SDA_PULLUP_ENABLED]))
    cg.add(var.set_scl_pin(config[CONF_SCL]))
    if CONF_SCL_PULLUP_ENABLED in config:
        cg.add(var.set_scl_pullup_enabled(config[CONF_SCL_PULLUP_ENABLED]))

    cg.add(var.set_frequency(int(config[CONF_FREQUENCY])))
    cg.add(var.set_scan(config[CONF_SCAN]))
    if CORE.using_arduino:
        cg.add_library("Wire", None)


def i2c_device_schema(default_address):
    """Create a schema for a i2c device.

    :param default_address: The default address of the i2c device, can be None to represent
      a required option.
    :return: The i2c device schema, `extend` this in your config schema.
    """
    schema = {
        cv.GenerateID(CONF_I2C_ID): cv.use_id(I2CBus),
        cv.Optional("multiplexer"): cv.invalid(
            "This option has been removed, please see "
            "the tca9584a docs for the updated way to use multiplexers"
        ),
    }
    if default_address is None:
        schema[cv.Required(CONF_ADDRESS)] = cv.i2c_address
    else:
        schema[cv.Optional(CONF_ADDRESS, default=default_address)] = cv.i2c_address
    return cv.Schema(schema)


async def register_i2c_device(var, config):
    """Register an i2c device with the given config.

    Sets the i2c bus to use and the i2c address.

    This is a coroutine, you need to await it with a 'yield' expression!
    """
    parent = await cg.get_variable(config[CONF_I2C_ID])
    cg.add(var.set_i2c_bus(parent))
    cg.add(var.set_i2c_address(config[CONF_ADDRESS]))
