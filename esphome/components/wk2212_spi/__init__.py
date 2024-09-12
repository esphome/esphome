import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, weikai
from esphome.const import (
    CONF_ID,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
)

CODEOWNERS = ["@DrCoolZic"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["weikai", "weikai_spi"]
MULTI_CONF = True
CONF_WK2212_SPI = "wk2212_spi"

weikai_ns = cg.esphome_ns.namespace("weikai")
weikai_spi_ns = cg.esphome_ns.namespace("weikai_spi")
WeikaiComponentSPI = weikai_spi_ns.class_(
    "WeikaiComponentSPI", weikai.WeikaiComponent, spi.SPIDevice
)
WeikaiGPIOPin = weikai_ns.class_(
    "WeikaiGPIOPin", cg.GPIOPin, cg.Parented.template(WeikaiComponentSPI)
)

CONFIG_SCHEMA = cv.All(
    weikai.WKBASE_SCHEMA.extend(
        {cv.GenerateID(): cv.declare_id(WeikaiComponentSPI)}
    ).extend(spi.spi_device_schema()),
    weikai.check_channel_max_2,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_name(str(config[CONF_ID])))
    await weikai.register_weikai(var, config)
    await spi.register_spi_device(var, config)


WK2212_PIN_SCHEMA = cv.All(
    weikai.WEIKAI_PIN_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(WeikaiGPIOPin),
            cv.Required(CONF_WK2212_SPI): cv.use_id(WeikaiComponentSPI),
        },
    ),
    weikai.validate_pin_mode,
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_WK2212_SPI, WK2212_PIN_SCHEMA)
async def sc16is75x_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_WK2212_SPI])
    cg.add(var.set_parent(parent))
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
