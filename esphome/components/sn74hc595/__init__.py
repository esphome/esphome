import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome.const import (
    CONF_ID,
    CONF_SPI_ID,
    CONF_MODE,
    CONF_NUMBER,
    CONF_INVERTED,
    CONF_DATA_PIN,
    CONF_CLOCK_PIN,
    CONF_OUTPUT,
)

DEPENDENCIES = []
AUTO_LOAD = ["spi"]
MULTI_CONF = True

sn74hc595_ns = cg.esphome_ns.namespace("sn74hc595")

SN74HC595Component = sn74hc595_ns.class_(
    "SN74HC595Component", cg.Component, spi.SPIDevice
)
SN74HC595GPIOPin = sn74hc595_ns.class_(
    "SN74HC595GPIOPin", cg.GPIOPin, cg.Parented.template(SN74HC595Component)
)

CONF_SN74HC595 = "sn74hc595"
CONF_LATCH_PIN = "latch_pin"
CONF_OE_PIN = "oe_pin"
CONF_SR_COUNT = "sr_count"


def _validate_config(config):
    if CONF_DATA_PIN in config or CONF_CLOCK_PIN in config:
        # Not using SPI
        del config[CONF_SPI_ID]

        if CONF_DATA_PIN not in config:
            raise cv.Invalid(f"{CONF_DATA_PIN} missing but {CONF_CLOCK_PIN} specified")

        if CONF_CLOCK_PIN not in config:
            raise cv.Invalid(f"{CONF_CLOCK_PIN} missing but {CONF_DATA_PIN} specified")

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(SN74HC595Component),
            cv.Optional(CONF_DATA_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_LATCH_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_OE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_SR_COUNT, default=1): cv.int_range(min=1, max=256),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=False)),
    _validate_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_DATA_PIN in config and CONF_CLOCK_PIN in config:
        data_pin = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
        cg.add(var.set_data_pin(data_pin))
        clock_pin = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
        cg.add(var.set_clock_pin(clock_pin))
    else:
        await spi.register_spi_device(var, config)
        cg.add(var.use_spi())
    latch_pin = await cg.gpio_pin_expression(config[CONF_LATCH_PIN])
    cg.add(var.set_latch_pin(latch_pin))
    if CONF_OE_PIN in config:
        oe_pin = await cg.gpio_pin_expression(config[CONF_OE_PIN])
        cg.add(var.set_oe_pin(oe_pin))
    cg.add(var.set_sr_count(config[CONF_SR_COUNT]))


def _validate_output_mode(value):
    if value is not True:
        raise cv.Invalid("Only output mode is supported")
    return value


SN74HC595_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(SN74HC595GPIOPin),
        cv.Required(CONF_SN74HC595): cv.use_id(SN74HC595Component),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=2048, max_included=False),
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_OUTPUT, default=True): cv.All(
                    cv.boolean, _validate_output_mode
                ),
            },
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_SN74HC595, SN74HC595_PIN_SCHEMA)
async def sn74hc595_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_SN74HC595])

    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    return var
