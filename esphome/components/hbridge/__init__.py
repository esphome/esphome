import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import (
    CONF_PIN_A,
    CONF_PIN_B,
    CONF_ENABLE_PIN,
    CONF_DECAY_MODE,
)

CODEOWNERS = ["@FaBjE"]

hbridge_ns = cg.esphome_ns.namespace("hbridge")

HBridge = hbridge_ns.class_("HBridge", cg.Component)


CurrentDecayMode = hbridge_ns.enum("CurrentDecayMode", is_class=True)
DECAY_MODE_OPTIONS = {
    "SLOW": CurrentDecayMode.SLOW,
    "FAST": CurrentDecayMode.FAST,
}

HBRIDGE_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PIN_A): cv.use_id(output.FloatOutput),
        cv.Required(CONF_PIN_B): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_ENABLE_PIN): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_DECAY_MODE, default="SLOW"): cv.enum(
            DECAY_MODE_OPTIONS, upper=True
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def hbridge_setup(config, var):

    # HBridge driver config
    pina = await cg.get_variable(config[CONF_PIN_A])
    cg.add(var.set_hbridge_pin_a(pina))
    pinb = await cg.get_variable(config[CONF_PIN_B])
    cg.add(var.set_hbridge_pin_b(pinb))

    if CONF_ENABLE_PIN in config:
        pin_enable = await cg.get_variable(config[CONF_ENABLE_PIN])
        cg.add(var.set_hbridge_enable_pin(pin_enable))

    cg.add(var.set_hbridge_decay_mode(config[CONF_DECAY_MODE]))
