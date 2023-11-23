import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome import automation

from esphome.const import (
    CONF_READ_PIN,
    CONF_ID,
    CONF_NAME,
    CONF_WRITE_PIN,
    CONF_REPEAT,
    CONF_INVERTED,
    CONF_PULSE_LENGTH,
    CONF_CODE,
)
from esphome.cpp_helpers import gpio_pin_expression

CODEOWNERS = ["@max246"]

lightwaverf_ns = cg.esphome_ns.namespace("lightwaverf")


LIGHTWAVERFComponent = lightwaverf_ns.class_(
    "LightWaveRF", cg.Component, cg.PollingComponent
)
LightwaveRawAction = lightwaverf_ns.class_("SendRawAction", automation.Action)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LIGHTWAVERFComponent),
        cv.Optional(CONF_READ_PIN, default=13): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_WRITE_PIN, default=14): pins.internal_gpio_input_pin_schema,
    }
).extend(cv.polling_component_schema("1s"))


LIGHTWAVE_SEND_SCHEMA = cv.Any(
    cv.int_range(min=1),
    cv.Schema(
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
    ),
)


@automation.register_action(
    "lightwaverf.send_raw",
    LightwaveRawAction,
    LIGHTWAVE_SEND_SCHEMA,
)
async def send_raw_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    repeats = await cg.templatable(config[CONF_REPEAT], args, int)
    inverted = await cg.templatable(config[CONF_INVERTED], args, bool)
    pulse_length = await cg.templatable(config[CONF_PULSE_LENGTH], args, int)
    code = config[CONF_CODE]

    cg.add(var.set_repeats(repeats))
    cg.add(var.set_inverted(inverted))
    cg.add(var.set_pulse_length(pulse_length))
    cg.add(var.set_data(code))
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin_read = await gpio_pin_expression(config[CONF_READ_PIN])
    pin_write = await gpio_pin_expression(config[CONF_WRITE_PIN])
    cg.add(var.set_pin(pin_write, pin_read))
