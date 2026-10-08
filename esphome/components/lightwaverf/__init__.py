from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_CODE,
    CONF_ID,
    CONF_INVERTED,
    CONF_NAME,
    CONF_PULSE_LENGTH,
    CONF_READ_PIN,
    CONF_REPEAT,
    CONF_WRITE_PIN,
)
from esphome.cpp_helpers import gpio_pin_expression
from esphome.types import ConfigType

CODEOWNERS = ["@max246"]

lightwaverf_ns = cg.esphome_ns.namespace("lightwaverf")


LIGHTWAVERFComponent = lightwaverf_ns.class_(
    "LightWaveRF", cg.Component, cg.PollingComponent
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LIGHTWAVERFComponent),
        cv.Optional(CONF_READ_PIN, default=13): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_WRITE_PIN, default=14): pins.internal_gpio_output_pin_schema,
    }
).extend(cv.polling_component_schema("1s"))


LIGHTWAVE_SEND_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(LIGHTWAVERFComponent),
        cv.Required(CONF_NAME): cv.string,
        cv.Required(CONF_CODE): cv.All(
            [cv.Any(cv.hex_uint8_t)],
            cv.Length(min=10),
        ),
        cv.Optional(CONF_REPEAT, default=10): cv.int_,
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        cv.Optional(CONF_PULSE_LENGTH, default=330): cv.int_,
    }
)


automation.register_apply_action(
    "lightwaverf.send_raw",
    LIGHTWAVE_SEND_SCHEMA,
    automation.ApplyCall(
        "send_rx({}, {}, {}, {})",
        (
            (CONF_CODE, cg.std_vector.template(cg.uint8)),
            (CONF_REPEAT, cg.uint8),
            (CONF_INVERTED, cg.bool_),
            (CONF_PULSE_LENGTH, cg.int_),
        ),
    ),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin_read = await gpio_pin_expression(config[CONF_READ_PIN])
    pin_write = await gpio_pin_expression(config[CONF_WRITE_PIN])
    cg.add(var.set_pin(pin_write, pin_read))
