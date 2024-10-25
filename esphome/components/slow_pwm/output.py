from esphome import automation, core, pins
import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PERIOD,
    CONF_PIN,
    CONF_TURN_OFF_ACTION,
    CONF_TURN_ON_ACTION,
)

slow_pwm_ns = cg.esphome_ns.namespace("slow_pwm")
SlowPWMOutput = slow_pwm_ns.class_("SlowPWMOutput", output.FloatOutput, cg.Component)

CONF_STATE_CHANGE_ACTION = "state_change_action"
CONF_RESTART_CYCLE_ON_STATE_CHANGE = "restart_cycle_on_state_change"
CONF_MIN_TIME_ON = "min_time_on"
CONF_MIN_TIME_OFF = "min_time_off"
CONF_MAX_PERIOD = "max_period"


def has_time_to_turn_on_off(config):
    """Validates that the period is at least as great as min_time_on+min_time_off."""
    min_time_on = config[CONF_MIN_TIME_ON].total_milliseconds
    min_time_off = config[CONF_MIN_TIME_OFF].total_milliseconds
    on_off_time = core.TimePeriod(milliseconds=min_time_on + min_time_off)

    period = config[CONF_PERIOD]
    max_period = config.get(CONF_MAX_PERIOD)

    if period < on_off_time:
        raise cv.Invalid(
            "The cycle period must be large enough to allow at least one turn on and one turn off"
        )

    if CONF_MAX_PERIOD in config and max_period:
        if max_period < on_off_time:
            raise cv.Invalid(
                "The max cycle period must be large enough to allow at least one turn on and one turn off"
            )

        if max_period < period:
            raise cv.Invalid("The max period must be larger than the default period")

    return config


CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(SlowPWMOutput),
            cv.Optional(CONF_PIN): pins.gpio_output_pin_schema,
            cv.Inclusive(
                CONF_TURN_ON_ACTION,
                "on_off",
                f"{CONF_TURN_ON_ACTION} and {CONF_TURN_OFF_ACTION} must both be defined",
            ): automation.validate_automation(single=True),
            cv.Inclusive(
                CONF_TURN_OFF_ACTION,
                "on_off",
                f"{CONF_TURN_ON_ACTION} and {CONF_TURN_OFF_ACTION} must both be defined",
            ): automation.validate_automation(single=True),
            cv.Optional(CONF_STATE_CHANGE_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Required(CONF_PERIOD): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=core.TimePeriod(milliseconds=100)),
            ),
            cv.Optional(CONF_RESTART_CYCLE_ON_STATE_CHANGE, default=False): cv.boolean,
            cv.Optional(
                CONF_MIN_TIME_ON, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_MIN_TIME_OFF, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MAX_PERIOD): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    has_time_to_turn_on_off,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    if CONF_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_PIN])
        cg.add(var.set_pin(pin))
    if CONF_STATE_CHANGE_ACTION in config:
        await automation.build_automation(
            var.get_state_change_trigger(),
            [(bool, "state")],
            config[CONF_STATE_CHANGE_ACTION],
        )
    if CONF_TURN_ON_ACTION in config:
        await automation.build_automation(
            var.get_turn_on_trigger(), [], config[CONF_TURN_ON_ACTION]
        )
        await automation.build_automation(
            var.get_turn_off_trigger(), [], config[CONF_TURN_OFF_ACTION]
        )

    cg.add(var.set_period(config[CONF_PERIOD]))
    cg.add(
        var.set_restart_cycle_on_state_change(
            config[CONF_RESTART_CYCLE_ON_STATE_CHANGE]
        )
    )

    cg.add(var.set_min_time_on(config[CONF_MIN_TIME_ON]))
    cg.add(var.set_min_time_off(config[CONF_MIN_TIME_OFF]))

    if CONF_MAX_PERIOD not in config:
        config[CONF_MAX_PERIOD] = 0
    cg.add(var.set_max_period(config[CONF_MAX_PERIOD]))
