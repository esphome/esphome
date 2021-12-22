import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import output
from esphome.const import CONF_ID, CONF_MIN_POWER, CONF_METHOD

CODEOWNERS = ["@glmnet"]

ac_dimmer_ns = cg.esphome_ns.namespace("ac_dimmer")
AcDimmer = ac_dimmer_ns.class_("AcDimmer", output.FloatOutput, cg.Component)

DimMethod = ac_dimmer_ns.enum("DimMethod")
DIM_METHODS = {
    "LEADING_PULSE": DimMethod.DIM_METHOD_LEADING_PULSE,
    "LEADING": DimMethod.DIM_METHOD_LEADING,
    "TRAILING": DimMethod.DIM_METHOD_TRAILING,
}

CONF_GATE_PIN = "gate_pin"
CONF_ZERO_CROSS_PIN = "zero_cross_pin"
CONF_INIT_WITH_HALF_CYCLE = "init_with_half_cycle"
CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(AcDimmer),
            cv.Required(CONF_GATE_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_ZERO_CROSS_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_INIT_WITH_HALF_CYCLE, default=True): cv.boolean,
            cv.Optional(CONF_METHOD, default="leading pulse"): cv.enum(
                DIM_METHODS, upper=True, space="_"
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # override default min power to 10%
    if CONF_MIN_POWER not in config:
        config[CONF_MIN_POWER] = 0.1
    await output.register_output(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_GATE_PIN])
    cg.add(var.set_gate_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_ZERO_CROSS_PIN])
    cg.add(var.set_zero_cross_pin(pin))
    cg.add(var.set_init_with_half_cycle(config[CONF_INIT_WITH_HALF_CYCLE]))
    cg.add(var.set_method(config[CONF_METHOD]))
