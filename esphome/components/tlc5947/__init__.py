# this component is for the "TLC5947 24-Channel, 12-Bit PWM LED Driver" [https://www.ti.com/lit/ds/symlink/tlc5947.pdf],
# which is used e.g. on [https://www.adafruit.com/product/1429]. The code is based on the components sm2135 and sm26716.

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_ID,
    CONF_NUM_CHIPS,
    CONF_OE_PIN,
)

CONF_LAT_PIN = "lat_pin"

CODEOWNERS = ["@rnauber"]

tlc5947_ns = cg.esphome_ns.namespace("tlc5947")
TLC5947 = tlc5947_ns.class_("TLC5947", cg.Component)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TLC5947),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_LAT_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_OE_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_NUM_CHIPS, default=1): cv.int_range(min=1, max=85),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    data = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(data))
    clock = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_clock_pin(clock))
    lat = await cg.gpio_pin_expression(config[CONF_LAT_PIN])
    cg.add(var.set_lat_pin(lat))
    if CONF_OE_PIN in config:
        outenable = await cg.gpio_pin_expression(config[CONF_OE_PIN])
        cg.add(var.set_outenable_pin(outenable))

    cg.add(var.set_num_chips(config[CONF_NUM_CHIPS]))
