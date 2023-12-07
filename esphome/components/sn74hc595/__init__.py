import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome.const import (
    CONF_ID,
    CONF_SPI_ID,
    CONF_NUMBER,
    CONF_INVERTED,
    CONF_DATA_PIN,
    CONF_CLOCK_PIN,
    CONF_OUTPUT,
)
from esphome.core import EsphomeError

MULTI_CONF = True

sn74hc595_ns = cg.esphome_ns.namespace("sn74hc595")

SN74HC595Component = sn74hc595_ns.class_("SN74HC595Component", cg.Component)
SN74HC595GPIOComponent = sn74hc595_ns.class_(
    "SN74HC595GPIOComponent", SN74HC595Component
)
SN74HC595SPIComponent = sn74hc595_ns.class_(
    "SN74HC595SPIComponent", SN74HC595Component, spi.SPIDevice
)

SN74HC595GPIOPin = sn74hc595_ns.class_(
    "SN74HC595GPIOPin", cg.GPIOPin, cg.Parented.template(SN74HC595Component)
)

CONF_SN74HC595 = "sn74hc595"
CONF_LATCH_PIN = "latch_pin"
CONF_OE_PIN = "oe_pin"
CONF_SR_COUNT = "sr_count"

CONFIG_SCHEMA = cv.Any(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(SN74HC595GPIOComponent),
            cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_LATCH_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_OE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_SR_COUNT, default=1): cv.int_range(min=1, max=256),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(SN74HC595SPIComponent),
            cv.Required(CONF_LATCH_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_OE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_SR_COUNT, default=1): cv.int_range(min=1, max=256),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=False))
    .extend(
        {
            cv.Required(CONF_SPI_ID): cv.use_id(spi.SPIComponent),
        }
    ),
    msg='Either "data_pin" and "clock_pin" must be set or "spi_id" must be set.',
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_DATA_PIN in config:
        data_pin = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
        cg.add(var.set_data_pin(data_pin))
        clock_pin = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
        cg.add(var.set_clock_pin(clock_pin))
    elif CONF_SPI_ID in config:
        await spi.register_spi_device(var, config)
    else:
        raise EsphomeError("Not supported")

    latch_pin = await cg.gpio_pin_expression(config[CONF_LATCH_PIN])
    cg.add(var.set_latch_pin(latch_pin))
    if CONF_OE_PIN in config:
        oe_pin = await cg.gpio_pin_expression(config[CONF_OE_PIN])
        cg.add(var.set_oe_pin(oe_pin))
    cg.add(var.set_sr_count(config[CONF_SR_COUNT]))


def _validate_output_mode(value):
    if value.get(CONF_OUTPUT) is not True:
        raise cv.Invalid("Only output mode is supported")
    return value


SN74HC595_PIN_SCHEMA = pins.gpio_base_schema(
    SN74HC595GPIOPin,
    cv.int_range(min=0, max=2047),
    modes=[CONF_OUTPUT],
    mode_validator=_validate_output_mode,
    invertable=True,
).extend(
    {
        cv.Required(CONF_SN74HC595): cv.use_id(SN74HC595Component),
    }
)


def sn74hc595_pin_final_validate(pin_config, parent_config):
    max_pins = parent_config[CONF_SR_COUNT] * 8
    if pin_config[CONF_NUMBER] >= max_pins:
        raise cv.Invalid(f"Pin number must be less than {max_pins}")


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_SN74HC595, SN74HC595_PIN_SCHEMA, sn74hc595_pin_final_validate
)
async def sn74hc595_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_SN74HC595])

    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    return var
