from esphome import pins, core
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_PIN,
    CONF_PERIOD,
    CONF_TURN_ON_ACTION,
    CONF_TURN_OFF_ACTION,
)

slow_pwm_ns = cg.esphome_ns.namespace("slow_pwm")
SlowPWMOutput = slow_pwm_ns.class_("SlowPWMOutput", output.FloatOutput, cg.Component)

CONF_STATE_CHANGE_ACTION = "state_change_action"
CONF_RESTART_CYCLE_ON_STATE_CHANGE = "restart_cycle_on_state_change"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
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
    }
).extend(cv.COMPONENT_SCHEMA)


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
