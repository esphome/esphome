import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_PIN_A, CONF_PIN_B, CONF_ENABLE_PIN

hbridge_ns = cg.esphome_ns.namespace("hbridge")

CONF_TRANSITION_DELTA_PER_MS = "transition_delta_per_ms"
CONF_TRANSITION_SHORT_BUILDUP_DURATION = "transition_short_buildup_duration"
CONF_TRANSITION_FULL_SHORT_DURATION = "transition_full_short_duration"

HBRIDGE_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PIN_A): cv.use_id(output.FloatOutput),
        cv.Required(CONF_PIN_B): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_ENABLE_PIN): cv.use_id(output.FloatOutput),

        cv.Optional(CONF_TRANSITION_DELTA_PER_MS, default="2"): cv.float_range(min=0, min_included=False),
        cv.Optional(CONF_TRANSITION_SHORT_BUILDUP_DURATION, default="0ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TRANSITION_FULL_SHORT_DURATION, default="0ms"): cv.positive_time_period_milliseconds,
    })


async def hbridge_config_to_code(config, var):

    # HBridge driver config
    pina = await cg.get_variable(config[CONF_PIN_A])
    cg.add(var.set_hbridge_pin_a(pina))
    pinb = await cg.get_variable(config[CONF_PIN_B])
    cg.add(var.set_hbridge_pin_b(pinb))

    if CONF_ENABLE_PIN in config:
        pin_enable = await cg.get_variable(config[CONF_ENABLE_PIN])
        cg.add(var.set_hbridge_enable_pin(pin_enable))

    #Transition settings
    if CONF_TRANSITION_DELTA_PER_MS in config:
        cg.add(var.set_setting_transition_delta_per_ms(config[CONF_TRANSITION_DELTA_PER_MS]))

    if CONF_TRANSITION_SHORT_BUILDUP_DURATION in config:
        cg.add(var.set_setting_transition_shorting_buildup_duration_ms(config[CONF_TRANSITION_SHORT_BUILDUP_DURATION]))

    if CONF_TRANSITION_FULL_SHORT_DURATION in config:
        cg.add(var.set_setting_transition_full_short_duration_ms(config[CONF_TRANSITION_FULL_SHORT_DURATION]))
