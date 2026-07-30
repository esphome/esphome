from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.i2c import I2CBus
import esphome.config_validation as cv
from esphome.const import (
    CONF_I2C_ID,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
)
from esphome.core import CORE

CODEOWNERS = ["@dwmw2"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
ch423_ns = cg.esphome_ns.namespace("ch423")

CH423Component = ch423_ns.class_("CH423Component", cg.Component, i2c.I2CDevice)
CH423GPIOPin = ch423_ns.class_(
    "CH423GPIOPin", cg.GPIOPin, cg.Parented.template(CH423Component)
)

CONF_CH423 = "ch423"

# Note that no address is configurable - each register in the CH423 has a dedicated i2c address
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(CH423Component),
        cv.GenerateID(CONF_I2C_ID): cv.use_id(I2CBus),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # Can't use register_i2c_device because there is no CONF_ADDRESS
    parent = await cg.get_variable(config[CONF_I2C_ID])
    cg.add(var.set_i2c_bus(parent))


# This is used as a final validation step so that modes have been fully transformed.
def pin_mode_check(pin_config, _):
    if pin_config[CONF_MODE][CONF_INPUT] and pin_config[CONF_NUMBER] >= 8:
        raise cv.Invalid("CH423 only supports input on pins 0-7")
    if pin_config[CONF_MODE][CONF_OPEN_DRAIN] and pin_config[CONF_NUMBER] < 8:
        raise cv.Invalid("CH423 only supports open drain output on pins 8-23")

    ch423_id = pin_config[CONF_CH423]
    pin_num = pin_config[CONF_NUMBER]
    is_output = pin_config[CONF_MODE][CONF_OUTPUT]
    is_open_drain = pin_config[CONF_MODE][CONF_OPEN_DRAIN]

    # Track pin modes per CH423 instance in CORE.data
    ch423_modes = CORE.data.setdefault(CONF_CH423, {})
    if ch423_id not in ch423_modes:
        ch423_modes[ch423_id] = {"gpio_output": None, "gpo_open_drain": None}

    if pin_num < 8:
        # GPIO pins (0-7): all must have same direction
        if ch423_modes[ch423_id]["gpio_output"] is None:
            ch423_modes[ch423_id]["gpio_output"] = is_output
        elif ch423_modes[ch423_id]["gpio_output"] != is_output:
            raise cv.Invalid(
                "CH423 GPIO pins (0-7) must all be configured as input or all as output"
            )
    # GPO pins (8-23): all must have same open-drain setting
    elif ch423_modes[ch423_id]["gpo_open_drain"] is None:
        ch423_modes[ch423_id]["gpo_open_drain"] = is_open_drain
    elif ch423_modes[ch423_id]["gpo_open_drain"] != is_open_drain:
        raise cv.Invalid(
            "CH423 GPO pins (8-23) must all be configured as push-pull or all as open-drain"
        )


CH423_PIN_SCHEMA = pins.gpio_base_schema(
    CH423GPIOPin,
    cv.int_range(min=0, max=23),
    modes=[CONF_INPUT, CONF_OUTPUT, CONF_OPEN_DRAIN],
).extend(
    {
        cv.Required(CONF_CH423): cv.use_id(CH423Component),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_CH423, CH423_PIN_SCHEMA, pin_mode_check)
async def ch423_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_CH423])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
