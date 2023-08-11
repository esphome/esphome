import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_NUMBER,
    CONF_INVERTED,
    CONF_CLOCK_PIN,
    CONF_OUTPUT,
    CONF_INPUT,
    CONF_ALLOW_OTHER_USES,
)

DEPENDENCIES = []
MULTI_CONF = True

hybridshift_ns = cg.esphome_ns.namespace("hybridshiftreg")

HybridShiftComponent = hybridshift_ns.class_("HybridShiftComponent", cg.Component)
HybridShiftGPIOPin = hybridshift_ns.class_(
    "HybridShiftGPIOPin", cg.GPIOPin, cg.Parented.template(HybridShiftComponent)
)

CONF_HYBRIDSHIFTREG = "hybridshiftreg"
CONF_DATA_OUT_PIN = "data_out_pin"
CONF_DATA_IN_PIN = "data_in_pin"
CONF_INH_LATCH_PIN = "inh_latch_pin"
CONF_LOAD_OE_PIN = "load_oe_pin"
CONF_SR_COUNT = "sr_count"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(HybridShiftComponent),
        cv.Required(CONF_DATA_OUT_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DATA_IN_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_INH_LATCH_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_LOAD_OE_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_SR_COUNT, default=1): cv.int_range(min=1, max=256),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    data_out_pin = await cg.gpio_pin_expression(config[CONF_DATA_OUT_PIN])
    cg.add(var.set_data_out_pin(data_out_pin))
    data_in_pin = await cg.gpio_pin_expression(config[CONF_DATA_IN_PIN])
    cg.add(var.set_data_in_pin(data_in_pin))
    clock_pin = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_clock_pin(clock_pin))
    inh_latch_pin = await cg.gpio_pin_expression(config[CONF_INH_LATCH_PIN])
    cg.add(var.set_inh_latch_pin(inh_latch_pin))
    oe_pin = await cg.gpio_pin_expression(config[CONF_LOAD_OE_PIN])
    cg.add(var.set_load_oe_pin(oe_pin))
    cg.add(var.set_sr_count(config[CONF_SR_COUNT]))


def _validate_output_mode(value):
    return True


SN74HC595_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(HybridShiftGPIOPin),
        cv.Required(CONF_HYBRIDSHIFTREG): cv.use_id(HybridShiftComponent),
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=2048, max_included=False),
        cv.Optional(CONF_ALLOW_OTHER_USES, default=True): cv.boolean,
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_OUTPUT, default=True): cv.All(
                    cv.boolean, _validate_output_mode
                ),
                cv.Optional(CONF_INPUT, default=False): cv.All(
                    cv.boolean, _validate_output_mode
                ),
            },
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_HYBRIDSHIFTREG, SN74HC595_PIN_SCHEMA)
async def hybridshift_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_HYBRIDSHIFTREG])

    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    return var
