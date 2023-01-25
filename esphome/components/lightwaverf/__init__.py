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


LIGHTWAVERFComponent = lightwaverf_ns.class_("LightWaveRF", cg.Component , cg.PollingComponent)


'''
RemoteTransmitterActionBase = lightwaverf_ns.class_(
    "RemoteTransmitterActionBase", automation.Action
)
LightwaveRawAction = lightwaverf_ns.class_("LIGHTWAVERF", RemoteTransmitterActionBase)
'''

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LIGHTWAVERFComponent),
        cv.Optional(CONF_READ_PIN, default=13): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_WRITE_PIN, default=14): pins.internal_gpio_input_pin_schema,
    }
).extend(cv.polling_component_schema("1s"))


'''

def validate_rc_switch_raw_code(value):
    if not isinstance(value, list):
        raise cv.Invalid("All Lightwave rf raw codes must a list of hex (0x00,0x00)")
    else:
        valid_value = True
        for i, val in enumerate(value):
            if val < 0:
                valid_value = False
                break
        if not valid_value:
            raise cv.Invalid("One or more values in the code are negative")

    return value


LIGHTWAVE_SEND_SCHEMA = cv.Any(
    cv.int_range(min=1),
    cv.Schema(
        {
            cv.Required(CONF_NAME): cv.string,
            cv.Required(CONF_CODE): cv.All(
                [cv.Any(cv.hex_int)],
                cv.Length(min=10),
                validate_rc_switch_raw_code,
            ),
            cv.Optional(CONF_REPEAT, default=10): cv.int_,
        }
    ),
)


@automation.register_action(
    "send_raw",
    LightwaveRawAction,
    LIGHTWAVE_SEND_SCHEMA,
)

'''

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin_read = await gpio_pin_expression(config[CONF_READ_PIN])
    pin_write = await gpio_pin_expression(config[CONF_WRITE_PIN])
    cg.add(var.set_pin(pin_write, pin_read))
