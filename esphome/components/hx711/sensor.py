import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import (
    CONF_CLK_PIN,
    CONF_GAIN,
    CONF_ID,
    ICON_SCALE,
    STATE_CLASS_MEASUREMENT,
)

hx711_ns = cg.esphome_ns.namespace("hx711")
HX711Sensor = hx711_ns.class_("HX711Sensor", sensor.Sensor, cg.PollingComponent)

CONF_DOUT_PIN = "dout_pin"

HX711Gain = hx711_ns.enum("HX711Gain")
GAINS = {
    128: HX711Gain.HX711_GAIN_128,
    32: HX711Gain.HX711_GAIN_32,
    64: HX711Gain.HX711_GAIN_64,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        icon=ICON_SCALE,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(HX711Sensor),
            cv.Required(CONF_DOUT_PIN): pins.gpio_input_pin_schema,
            cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_GAIN, default=128): cv.enum(GAINS, int=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    dout_pin = await cg.gpio_pin_expression(config[CONF_DOUT_PIN])
    cg.add(var.set_dout_pin(dout_pin))
    sck_pin = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_sck_pin(sck_pin))
    cg.add(var.set_gain(config[CONF_GAIN]))
