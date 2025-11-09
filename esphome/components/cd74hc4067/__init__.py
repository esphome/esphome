from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_DELAY, CONF_ID

AUTO_LOAD = ["sensor", "voltage_sampler"]
CODEOWNERS = ["@asoehlke"]
MULTI_CONF = True

cd74hc4067_ns = cg.esphome_ns.namespace("cd74hc4067")

CD74HC4067Component = cd74hc4067_ns.class_(
    "CD74HC4067Component", cg.Component, cg.PollingComponent
)

CONF_PIN_S0 = "pin_s0"
CONF_PIN_S1 = "pin_s1"
CONF_PIN_S2 = "pin_s2"
CONF_PIN_S3 = "pin_s3"

DEFAULT_DELAY = "2ms"


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CD74HC4067Component),
        cv.Required(CONF_PIN_S0): pins.gpio_output_pin_schema,
        cv.Required(CONF_PIN_S1): pins.gpio_output_pin_schema,
        cv.Required(CONF_PIN_S2): pins.gpio_output_pin_schema,
        cv.Required(CONF_PIN_S3): pins.gpio_output_pin_schema,
        cv.Optional(
            CONF_DELAY, default=DEFAULT_DELAY
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin_s0 = await cg.gpio_pin_expression(config[CONF_PIN_S0])
    cg.add(var.set_pin_s0(pin_s0))
    pin_s1 = await cg.gpio_pin_expression(config[CONF_PIN_S1])
    cg.add(var.set_pin_s1(pin_s1))
    pin_s2 = await cg.gpio_pin_expression(config[CONF_PIN_S2])
    cg.add(var.set_pin_s2(pin_s2))
    pin_s3 = await cg.gpio_pin_expression(config[CONF_PIN_S3])
    cg.add(var.set_pin_s3(pin_s3))

    cg.add(var.set_switch_delay(config[CONF_DELAY]))
