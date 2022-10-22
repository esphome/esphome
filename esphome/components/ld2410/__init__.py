import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID
from esphome import automation
from esphome.automation import maybe_simple_id

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@sebcaps"]

ld2410_ns = cg.esphome_ns.namespace("ld2410")
LD2410Component = ld2410_ns.class_(
    "LD2410Component", cg.PollingComponent, uart.UARTDevice
)

LD2410SetConfigMode = ld2410_ns.class_("LD2410SetConfigMode", automation.Action)
CONF_LD2410_ID = "ld2410_id"
CONF_MAX_MOVE_DISTANCE = "max_move_distance"
CONF_MAX_STILL_DISTANCE = "max_still_distance"
CONF_NONE_DURATION = "none_duration"
CONF_G0_MOVE_SENSITIVITY = "g0_move_sensitivity"
CONF_G0_STILL_SENSITIVITY = "g0_still_sensitivity"
CONF_G1_MOVE_SENSITIVITY = "g1_move_sensitivity"
CONF_G1_STILL_SENSITIVITY = "g1_still_sensitivity"
CONF_G2_MOVE_SENSITIVITY = "g2_move_sensitivity"
CONF_G2_STILL_SENSITIVITY = "g2_still_sensitivity"
CONF_G3_MOVE_SENSITIVITY = "g3_move_sensitivity"
CONF_G3_STILL_SENSITIVITY = "g3_still_sensitivity"
CONF_G4_MOVE_SENSITIVITY = "g4_move_sensitivity"
CONF_G4_STILL_SENSITIVITY = "g4_still_sensitivity"
CONF_G5_MOVE_SENSITIVITY = "g5_move_sensitivity"
CONF_G5_STILL_SENSITIVITY = "g5_still_sensitivity"
CONF_G6_MOVE_SENSITIVITY = "g6_move_sensitivity"
CONF_G6_STILL_SENSITIVITY = "g6_still_sensitivity"
CONF_G7_MOVE_SENSITIVITY = "g7_move_sensitivity"
CONF_G7_STILL_SENSITIVITY = "g7_still_sensitivity"
CONF_G8_MOVE_SENSITIVITY = "g8_move_sensitivity"
CONF_G8_STILL_SENSITIVITY = "g8_still_sensitivity"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2410Component),
            cv.Optional(CONF_MAX_MOVE_DISTANCE, default=6): cv.int_range(min=1, max=8),
            cv.Optional(CONF_MAX_STILL_DISTANCE, default=6): cv.int_range(min=1, max=8),
            cv.Optional(CONF_NONE_DURATION, default=5): cv.int_range(min=0, max=32767),
            cv.Optional(CONF_G0_MOVE_SENSITIVITY, default=50): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G0_STILL_SENSITIVITY, default=0): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G1_MOVE_SENSITIVITY, default=50): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G1_STILL_SENSITIVITY, default=0): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G2_MOVE_SENSITIVITY, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G2_STILL_SENSITIVITY, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G3_MOVE_SENSITIVITY, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G3_STILL_SENSITIVITY, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G4_MOVE_SENSITIVITY, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G4_STILL_SENSITIVITY, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G5_MOVE_SENSITIVITY, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G5_STILL_SENSITIVITY, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G6_MOVE_SENSITIVITY, default=30): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G6_STILL_SENSITIVITY, default=15): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G7_MOVE_SENSITIVITY, default=30): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G7_STILL_SENSITIVITY, default=15): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G8_MOVE_SENSITIVITY, default=30): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G8_STILL_SENSITIVITY, default=15): cv.int_range(
                min=0, max=100
            ),
        }
    ).extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_none_duration(config[CONF_NONE_DURATION]))
    cg.add(var.set_max_move_distance(config[CONF_MAX_MOVE_DISTANCE]))
    cg.add(var.set_max_still_distance(config[CONF_MAX_STILL_DISTANCE]))
    cg.add(
        var.set_range_config(
            config[CONF_G0_MOVE_SENSITIVITY],
            config[CONF_G0_STILL_SENSITIVITY],
            config[CONF_G1_MOVE_SENSITIVITY],
            config[CONF_G1_STILL_SENSITIVITY],
            config[CONF_G2_MOVE_SENSITIVITY],
            config[CONF_G2_STILL_SENSITIVITY],
            config[CONF_G3_MOVE_SENSITIVITY],
            config[CONF_G3_STILL_SENSITIVITY],
            config[CONF_G4_MOVE_SENSITIVITY],
            config[CONF_G4_STILL_SENSITIVITY],
            config[CONF_G5_MOVE_SENSITIVITY],
            config[CONF_G5_STILL_SENSITIVITY],
            config[CONF_G6_MOVE_SENSITIVITY],
            config[CONF_G6_STILL_SENSITIVITY],
            config[CONF_G7_MOVE_SENSITIVITY],
            config[CONF_G7_STILL_SENSITIVITY],
            config[CONF_G8_MOVE_SENSITIVITY],
            config[CONF_G8_STILL_SENSITIVITY],
        )
    )


CALIBRATION_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(LD2410Component),
    }
)


@automation.register_action(
    "ld2410.set_config_mode", LD2410SetConfigMode, CALIBRATION_ACTION_SCHEMA
)
async def ld2410_set_config_mode(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
