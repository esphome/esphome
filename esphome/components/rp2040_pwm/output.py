from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_PIN
from esphome.types import ConfigType

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["rp2"]


rp2040_pwm_ns = cg.esphome_ns.namespace("rp2040_pwm")
RP2040PWM = rp2040_pwm_ns.class_("RP2040PWM", output.FloatOutput, cg.Component)
validate_frequency = cv.All(cv.frequency, cv.float_range(min=1.0e-6))

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(RP2040PWM),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_FREQUENCY, default="1kHz"): validate_frequency,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    cg.add(var.set_frequency(config[CONF_FREQUENCY]))


automation.register_apply_action(
    "output.rp2040_pwm.set_frequency",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(RP2040PWM),
            cv.Required(CONF_FREQUENCY): cv.templatable(validate_frequency),
        }
    ),
    automation.ApplyField(CONF_FREQUENCY, "update_frequency", cg.float_),
)
