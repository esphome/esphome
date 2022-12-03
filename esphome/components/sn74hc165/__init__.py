import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_ID,
    CONF_DATA_PIN,
    CONF_CLOCK_PIN,
)

CODEOWNERS = ["@alsig"]

DEPENDENCIES = []
MULTI_CONF = True
CONF_SN74HC165_ID = "sn74hc165_id"

sn74hc165_ns = cg.esphome_ns.namespace("sn74hc165")

SN74HC165Component = sn74hc165_ns.class_("SN74HC165Component", cg.Component)
SN74HC165GPIOPin = sn74hc165_ns.class_("SN74HC165GPIOPin", cg.GPIOPin)

CONF_SN74HC165 = "sn74hc165"
CONF_LATCH_PIN = "latch_pin"
CONF_SCAN_RATE = "scan_rate"
CONF_SR_COUNT = "sr_count"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(SN74HC165Component),
        cv.Required(CONF_DATA_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_LATCH_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_SR_COUNT, default=1): cv.int_range(1, 4),
        cv.Optional(CONF_SCAN_RATE, default=100): cv.int_range(1),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    data_pin = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(data_pin))
    clock_pin = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_clock_pin(clock_pin))
    latch_pin = await cg.gpio_pin_expression(config[CONF_LATCH_PIN])
    cg.add(var.set_latch_pin(latch_pin))
    if CONF_SCAN_RATE in config:
        cg.add(var.set_scan_rate(config[CONF_SCAN_RATE]))
    cg.add(var.set_sr_count(config[CONF_SR_COUNT]))
