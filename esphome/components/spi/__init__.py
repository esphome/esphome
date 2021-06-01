import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_CLK_PIN,
    CONF_ID,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_SPI_ID,
    CONF_CS_PIN,
)
from esphome.core import coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
spi_ns = cg.esphome_ns.namespace("spi")
SPIComponent = spi_ns.class_("SPIComponent", cg.Component)
SPIDevice = spi_ns.class_("SPIDevice")
MULTI_CONF = True

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPIComponent),
            cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_MISO_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_MOSI_PIN): pins.gpio_output_pin_schema,
        }
    ),
    cv.has_at_least_one_key(CONF_MISO_PIN, CONF_MOSI_PIN),
)


@coroutine_with_priority(1.0)
async def to_code(config):
    cg.add_global(spi_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    clk = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk(clk))
    if CONF_MISO_PIN in config:
        miso = await cg.gpio_pin_expression(config[CONF_MISO_PIN])
        cg.add(var.set_miso(miso))
    if CONF_MOSI_PIN in config:
        mosi = await cg.gpio_pin_expression(config[CONF_MOSI_PIN])
        cg.add(var.set_mosi(mosi))


def spi_device_schema(cs_pin_required=True):
    """Create a schema for an SPI device.
    :param cs_pin_required: If true, make the CS_PIN required in the config.
    :return: The SPI device schema, `extend` this in your config schema.
    """
    schema = {
        cv.GenerateID(CONF_SPI_ID): cv.use_id(SPIComponent),
    }
    if cs_pin_required:
        schema[cv.Required(CONF_CS_PIN)] = pins.gpio_output_pin_schema
    else:
        schema[cv.Optional(CONF_CS_PIN)] = pins.gpio_output_pin_schema
    return cv.Schema(schema)


async def register_spi_device(var, config):
    parent = await cg.get_variable(config[CONF_SPI_ID])
    cg.add(var.set_spi_parent(parent))
    if CONF_CS_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_CS_PIN])
        cg.add(var.set_cs_pin(pin))


def validate_device(name, config, item_config, require_mosi, require_miso):
    spi_config = config.get_config_by_id(item_config[CONF_SPI_ID])
    if require_mosi and CONF_MISO_PIN not in spi_config:
        raise ValueError(f"Component {name} requires parent spi to declare miso_pin")
    if require_miso and CONF_MOSI_PIN not in spi_config:
        raise ValueError(f"Component {name} requires parent spi to declare mosi_pin")
