import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_PIN_A, CONF_PIN_B, CONF_ENABLE_PIN, CONF_DECAY_MODE

CODEOWNERS = ["@FaBjE"]


hbridge_ns = cg.esphome_ns.namespace("hbridge")

HBridge = hbridge_ns.class_("HBridge", cg.Component)

CONF_TRANSITION_DELTA_PER_MS = "transition_delta_per_ms"
CONF_TRANSITION_SHORT_BUILDUP_DURATION = "transition_short_buildup_duration"
CONF_TRANSITION_FULL_SHORT_DURATION = "transition_full_short_duration"

CurrentDecayMode = hbridge_ns.enum("CurrentDecayMode", is_class=True)
DECAY_MODE_OPTIONS = {
    "SLOW": CurrentDecayMode.SLOW,
    "FAST": CurrentDecayMode.FAST,
}

HBRIDGE_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PIN_A): cv.use_id(output.FloatOutput),
        cv.Required(CONF_PIN_B): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_ENABLE_PIN): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_DECAY_MODE, default="SLOW"): cv.enum(
            DECAY_MODE_OPTIONS, upper=True
        ),
        cv.Optional(CONF_TRANSITION_DELTA_PER_MS, default="2"): cv.float_range(
            min=0, min_included=False
        ),
        cv.Optional(
            CONF_TRANSITION_SHORT_BUILDUP_DURATION, default="0ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_TRANSITION_FULL_SHORT_DURATION, default="0ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def hbridge_setup(config, var):

    # HBridge driver config
    pina = await cg.get_variable(config[CONF_PIN_A])
    cg.add(var.set_hbridge_pin_a(pina))
    pinb = await cg.get_variable(config[CONF_PIN_B])
    cg.add(var.set_hbridge_pin_b(pinb))

    if CONF_ENABLE_PIN in config:
        pin_enable = await cg.get_variable(config[CONF_ENABLE_PIN])
        cg.add(var.set_hbridge_enable_pin(pin_enable))

    cg.add(var.set_hbridge_decay_mode(config[CONF_DECAY_MODE]))

    # Transition settings
    cg.add(
        var.set_setting_transition_delta_per_ms(config[CONF_TRANSITION_DELTA_PER_MS])
    )

    cg.add(
        var.set_setting_transition_shorting_buildup_duration_ms(
            config[CONF_TRANSITION_SHORT_BUILDUP_DURATION]
        )
    )

    cg.add(
        var.set_setting_transition_full_short_duration_ms(
            config[CONF_TRANSITION_FULL_SHORT_DURATION]
        )
    )
