from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN, CONF_TURN_OFF_ACTION, CONF_TURN_ON_ACTION

DEPENDENCIES = []


sigma_delta_output_ns = cg.esphome_ns.namespace("sigma_delta_output")
SigmaDeltaOutput = sigma_delta_output_ns.class_(
    "SigmaDeltaOutput", output.FloatOutput, cg.PollingComponent
)

CONF_STATE_CHANGE_ACTION = "state_change_action"

CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(cv.polling_component_schema("60s")).extend(
        {
            cv.Required(CONF_ID): cv.declare_id(SigmaDeltaOutput),
            cv.Optional(CONF_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_STATE_CHANGE_ACTION): automation.validate_automation(
                single=True
            ),
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
        }
    ),
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
