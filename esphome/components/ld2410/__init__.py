import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID
from esphome import automation
from esphome.automation import maybe_simple_id

DEPENDENCIES = ["uart"]

ld2410_ns = cg.esphome_ns.namespace("ld2410")
LD2410Component = ld2410_ns.class_(
    "LD2410Component", cg.PollingComponent, uart.UARTDevice
)

LD2410SetConfigMode = ld2410_ns.class_("LD2410SetConfigMode", automation.Action)
CONF_LD2410_ID = "ld2410_id"
CONF_LD2410_MAXMOVEDISTANCE = "max_move_distance"
CONF_LD2410_MAXSTILLDISTANCE = "max_still_distance"
CONF_LD2410_NONE_DURATION = "no_one_duration"
CONF_LD2410_G0_MOVE = "g0_move_sensibility"
CONF_LD2410_G0_STILL = "g0_still_sensibility"
CONF_LD2410_G1_MOVE = "g1_move_sensibility"
CONF_LD2410_G1_STILL = "g1_still_sensibility"
CONF_LD2410_G2_MOVE = "g2_move_sensibility"
CONF_LD2410_G2_STILL = "g2_still_sensibility"
CONF_LD2410_G3_MOVE = "g3_move_sensibility"
CONF_LD2410_G3_STILL = "g3_still_sensibility"
CONF_LD2410_G4_MOVE = "g4_move_sensibility"
CONF_LD2410_G4_STILL = "g4_still_sensibility"
CONF_LD2410_G5_MOVE = "g5_move_sensibility"
CONF_LD2410_G5_STILL = "g5_still_sensibility"
CONF_LD2410_G6_MOVE = "g6_move_sensibility"
CONF_LD2410_G6_STILL = "g6_still_sensibility"
CONF_LD2410_G7_MOVE = "g7_move_sensibility"
CONF_LD2410_G7_STILL = "g7_still_sensibility"
CONF_LD2410_G8_MOVE = "g8_move_sensibility"
CONF_LD2410_G8_STILL = "g8_still_sensibility"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2410Component),
            cv.Optional(CONF_LD2410_MAXMOVEDISTANCE, default=6): cv.int_range(
                min=1, max=8
            ),
            cv.Optional(CONF_LD2410_MAXSTILLDISTANCE, default=6): cv.int_range(
                min=1, max=8
            ),
            cv.Optional(CONF_LD2410_NONE_DURATION, default=5): cv.int_range(
                min=0, max=32767
            ),
            cv.Optional(CONF_LD2410_G0_MOVE, default=50): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G0_STILL, default=0): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G1_MOVE, default=50): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G1_STILL, default=0): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G2_MOVE, default=40): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G2_STILL, default=40): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G3_MOVE, default=40): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G3_STILL, default=40): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G4_MOVE, default=40): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G4_STILL, default=40): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G5_MOVE, default=40): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G5_STILL, default=40): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G6_MOVE, default=30): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G6_STILL, default=15): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G7_MOVE, default=30): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G7_STILL, default=15): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G8_MOVE, default=30): cv.int_range(min=0, max=100),
            cv.Optional(CONF_LD2410_G8_STILL, default=15): cv.int_range(min=0, max=100),
        }
    ).extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.setNoneDuration(config[CONF_LD2410_NONE_DURATION]))
    cg.add(var.setMaxMoveDistance(config[CONF_LD2410_MAXMOVEDISTANCE]))
    cg.add(var.setMaxStillDistance(config[CONF_LD2410_MAXSTILLDISTANCE]))
    cg.add(
        var.setRangeConfig(
            config[CONF_LD2410_G0_MOVE],
            config[CONF_LD2410_G0_STILL],
            config[CONF_LD2410_G1_MOVE],
            config[CONF_LD2410_G1_STILL],
            config[CONF_LD2410_G2_MOVE],
            config[CONF_LD2410_G2_STILL],
            config[CONF_LD2410_G3_MOVE],
            config[CONF_LD2410_G3_STILL],
            config[CONF_LD2410_G4_MOVE],
            config[CONF_LD2410_G4_STILL],
            config[CONF_LD2410_G5_MOVE],
            config[CONF_LD2410_G5_STILL],
            config[CONF_LD2410_G6_MOVE],
            config[CONF_LD2410_G6_STILL],
            config[CONF_LD2410_G7_MOVE],
            config[CONF_LD2410_G7_STILL],
            config[CONF_LD2410_G8_MOVE],
            config[CONF_LD2410_G8_STILL],
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
