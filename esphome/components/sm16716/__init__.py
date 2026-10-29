from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_NUM_CHIPS,
)
from esphome.cpp_generator import MockObj, VariableDeclarationExpression
from esphome.types import ConfigType

AUTO_LOAD = ["output"]
DOMAIN = "sm16716"
sm16716_ns = cg.esphome_ns.namespace("sm16716")
SM16716 = sm16716_ns.class_("SM16716", cg.Component)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SM16716),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_NUM_CHANNELS, default=3): cv.int_range(min=3, max=255),
        cv.Optional(CONF_NUM_CHIPS, default=1): cv.int_range(min=1, max=85),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    # Duty values live in a static array sized from the config, so nothing is allocated at run time
    num_channels = config[CONF_NUM_CHANNELS]
    pwm_amounts = cg.unique_global_id(
        f"{config[CONF_ID]}__pwm_amounts",
        cg.std_ns.class_("array").template(cg.uint8, num_channels),
    )
    cg.add_global(
        VariableDeclarationExpression(pwm_amounts.type, "", pwm_amounts, static=True)
    )
    var = cg.new_Pvariable(
        config[CONF_ID],
        await cg.gpio_pin_expression(config[CONF_DATA_PIN]),
        await cg.gpio_pin_expression(config[CONF_CLOCK_PIN]),
        MockObj(pwm_amounts, ".").data(),
        num_channels,
        config[CONF_NUM_CHIPS],
    )
    await cg.register_component(var, config)
