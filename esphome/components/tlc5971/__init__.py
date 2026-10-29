# this component is for the "TLC5971 12-Channel, 12-Bit PWM LED Driver" [https://www.ti.com/lit/ds/symlink/tlc5971.pdf],
# which is used e.g. on [https://www.adafruit.com/product/1455]. The code is based on the TLC5947 component by @rnauber.

from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_CLOCK_PIN, CONF_DATA_PIN, CONF_ID, CONF_NUM_CHIPS
from esphome.cpp_generator import MockObj, VariableDeclarationExpression
from esphome.types import ConfigType

# Must match N_CHANNELS_PER_CHIP in tlc5971.h
CHANNELS_PER_CHIP = 12

CODEOWNERS = ["@IJIJI"]
DOMAIN = "tlc5971"

tlc5971_ns = cg.esphome_ns.namespace("tlc5971")
TLC5971 = tlc5971_ns.class_("TLC5971", cg.Component)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TLC5971),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_NUM_CHIPS, default=1): cv.int_range(min=1, max=85),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    # Duty values live in a static array sized from the config, so nothing is allocated at run time
    num_chips = config[CONF_NUM_CHIPS]
    pwm_amounts = cg.unique_global_id(
        f"{config[CONF_ID]}__pwm_amounts",
        cg.std_ns.class_("array").template(cg.uint16, num_chips * CHANNELS_PER_CHIP),
    )
    cg.add_global(
        VariableDeclarationExpression(pwm_amounts.type, "", pwm_amounts, static=True)
    )
    var = cg.new_Pvariable(
        config[CONF_ID],
        await cg.gpio_pin_expression(config[CONF_DATA_PIN]),
        await cg.gpio_pin_expression(config[CONF_CLOCK_PIN]),
        MockObj(pwm_amounts, ".").data(),
        num_chips,
    )
    await cg.register_component(var, config)
