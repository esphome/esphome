import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NUMBER, CONF_PIN
from esphome.types import ConfigType

from .. import (
    BASE_SCHEMA,
    CONF_M5PM1_ID,
    M5PM1_GPIO_OUTPUT_PIN_SCHEMA,
    PWM_POLARITIES,
    M5PM1PWMChannel,
    m5pm1_ns,
)

DEPENDENCIES = ["m5pm1"]

M5PM1FloatOutput = m5pm1_ns.class_("M5PM1FloatOutput", output.FloatOutput, cg.Component)

M5PM1_PWM_PIN_TO_CHANNEL = {
    3: M5PM1PWMChannel.M5PM1_PWM_CHANNEL_1,
    4: M5PM1PWMChannel.M5PM1_PWM_CHANNEL_2,
}

CONF_PWM_POLARITY = "pwm_polarity"


def validate_pwm_pin(pin: ConfigType) -> ConfigType:
    if pin[CONF_NUMBER] not in (3, 4):
        raise cv.Invalid(
            "M5PM1 PWM only supports GPIO3 and GPIO4",
            path=[CONF_NUMBER],
        )
    return pin


M5PM1_PWM_PIN_SCHEMA = cv.All(
    M5PM1_GPIO_OUTPUT_PIN_SCHEMA,
    validate_pwm_pin,
)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(M5PM1FloatOutput),
        cv.Required(CONF_PIN): M5PM1_PWM_PIN_SCHEMA,
        cv.Optional(CONF_PWM_POLARITY, default="ACTIVE_HIGH"): cv.enum(PWM_POLARITIES),
    }
).extend(BASE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    pin_number = config[CONF_PIN][CONF_NUMBER]
    channel = M5PM1_PWM_PIN_TO_CHANNEL[pin_number]
    cg.add(var.set_pin(pin))
    cg.add(var.set_channel(channel))
    cg.add(var.set_pwm_polarity(config[CONF_PWM_POLARITY]))

    await cg.register_parented(var, config[CONF_M5PM1_ID])
    await cg.register_component(var, config)
