from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PULLUP,
)

CODEOWNERS = ["@looping40"]

DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_BRIGHTNESS_MODE = "brightness_mode"
CONF_BRIGHTNESS_GLOBAL = "brightness_global"


max6956_ns = cg.esphome_ns.namespace("max6956")

MAX6956 = max6956_ns.class_("MAX6956", cg.Component, i2c.I2CDevice)
MAX6956GPIOPin = max6956_ns.class_("MAX6956GPIOPin", cg.GPIOPin)

# Actions
SetCurrentGlobalAction = max6956_ns.class_("SetCurrentGlobalAction", automation.Action)
SetCurrentModeAction = max6956_ns.class_("SetCurrentModeAction", automation.Action)

MAX6956_CURRENTMODE = max6956_ns.enum("MAX6956CURRENTMODE")
CURRENT_MODES = {
    "global": MAX6956_CURRENTMODE.GLOBAL,
    "segment": MAX6956_CURRENTMODE.SEGMENT,
}


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(MAX6956),
            cv.Optional(CONF_BRIGHTNESS_GLOBAL, default="0"): cv.int_range(
                min=0, max=15
            ),
            cv.Optional(CONF_BRIGHTNESS_MODE, default="global"): cv.enum(
                CURRENT_MODES, lower=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x40))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_brightness_mode(config[CONF_BRIGHTNESS_MODE]))
    cg.add(var.set_brightness_global(config[CONF_BRIGHTNESS_GLOBAL]))


def validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_PULLUP] and not value[CONF_INPUT]:
        raise cv.Invalid("Pullup only available with input")
    return value


CONF_MAX6956 = "max6956"

MAX6956_PIN_SCHEMA = pins.gpio_base_schema(
    MAX6956GPIOPin,
    cv.int_range(min=4, max=31),
    modes=[CONF_INPUT, CONF_PULLUP, CONF_OUTPUT],
    mode_validator=validate_mode,
).extend(
    {
        cv.Required(CONF_MAX6956): cv.use_id(MAX6956),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_MAX6956, MAX6956_PIN_SCHEMA)
async def max6956_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_MAX6956])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var


@automation.register_action(
    "max6956.set_brightness_global",
    SetCurrentGlobalAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(CONF_ID): cv.use_id(MAX6956),
            cv.Required(CONF_BRIGHTNESS_GLOBAL): cv.templatable(
                cv.int_range(min=0, max=15)
            ),
        },
        key=CONF_BRIGHTNESS_GLOBAL,
    ),
)
async def max6956_set_brightness_global_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_BRIGHTNESS_GLOBAL], args, float)
    cg.add(var.set_brightness_global(template_))
    return var


@automation.register_action(
    "max6956.set_brightness_mode",
    SetCurrentModeAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(MAX6956),
            cv.Required(CONF_BRIGHTNESS_MODE): cv.templatable(
                cv.enum(CURRENT_MODES, lower=True)
            ),
        },
        key=CONF_BRIGHTNESS_MODE,
    ),
)
async def max6956_set_brightness_mode_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_BRIGHTNESS_MODE], args, float)
    cg.add(var.set_brightness_mode(template_))
    return var
