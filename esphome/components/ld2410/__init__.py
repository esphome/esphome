import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_TIMEOUT
from esphome import automation
from esphome.automation import maybe_simple_id

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@sebcaps"]
MULTI_CONF = True

ld2410_ns = cg.esphome_ns.namespace("ld2410")
LD2410Component = ld2410_ns.class_("LD2410Component", cg.Component, uart.UARTDevice)
LD2410Restart = ld2410_ns.class_("LD2410Restart", automation.Action)
CONF_LD2410_ID = "ld2410_id"
CONF_MAX_MOVE_DISTANCE = "max_move_distance"
CONF_MAX_STILL_DISTANCE = "max_still_distance"
CONF_G0_MOVE_THRESHOLD = "g0_move_threshold"
CONF_G0_STILL_THRESHOLD = "g0_still_threshold"
CONF_G1_MOVE_THRESHOLD = "g1_move_threshold"
CONF_G1_STILL_THRESHOLD = "g1_still_threshold"
CONF_G2_MOVE_THRESHOLD = "g2_move_threshold"
CONF_G2_STILL_THRESHOLD = "g2_still_threshold"
CONF_G3_MOVE_THRESHOLD = "g3_move_threshold"
CONF_G3_STILL_THRESHOLD = "g3_still_threshold"
CONF_G4_MOVE_THRESHOLD = "g4_move_threshold"
CONF_G4_STILL_THRESHOLD = "g4_still_threshold"
CONF_G5_MOVE_THRESHOLD = "g5_move_threshold"
CONF_G5_STILL_THRESHOLD = "g5_still_threshold"
CONF_G6_MOVE_THRESHOLD = "g6_move_threshold"
CONF_G6_STILL_THRESHOLD = "g6_still_threshold"
CONF_G7_MOVE_THRESHOLD = "g7_move_threshold"
CONF_G7_STILL_THRESHOLD = "g7_still_threshold"
CONF_G8_MOVE_THRESHOLD = "g8_move_threshold"
CONF_G8_STILL_THRESHOLD = "g8_still_threshold"

DISTANCES = [0.75, 1.5, 2.25, 3, 3.75, 4.5, 5.25, 6]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2410Component),
            cv.Optional(CONF_MAX_MOVE_DISTANCE, default="4.5m"): cv.All(
                cv.distance, cv.one_of(*DISTANCES, float=True)
            ),
            cv.Optional(CONF_MAX_STILL_DISTANCE, default="4.5m"): cv.All(
                cv.distance, cv.one_of(*DISTANCES, float=True)
            ),
            cv.Optional(CONF_TIMEOUT, default="5s"): cv.All(
                cv.positive_time_period_seconds,
                cv.Range(max=cv.TimePeriod(seconds=32767)),
            ),
            cv.Optional(CONF_G0_MOVE_THRESHOLD, default=50): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G0_STILL_THRESHOLD, default=0): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G1_MOVE_THRESHOLD, default=50): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G1_STILL_THRESHOLD, default=0): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G2_MOVE_THRESHOLD, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G2_STILL_THRESHOLD, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G3_MOVE_THRESHOLD, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G3_STILL_THRESHOLD, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G4_MOVE_THRESHOLD, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G4_STILL_THRESHOLD, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G5_MOVE_THRESHOLD, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G5_STILL_THRESHOLD, default=40): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G6_MOVE_THRESHOLD, default=30): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G6_STILL_THRESHOLD, default=15): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G7_MOVE_THRESHOLD, default=30): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G7_STILL_THRESHOLD, default=15): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G8_MOVE_THRESHOLD, default=30): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_G8_STILL_THRESHOLD, default=15): cv.int_range(
                min=0, max=100
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ld2410",
    baud_rate=256000,
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_max_move_distance(int(config[CONF_MAX_MOVE_DISTANCE] / 0.75)))
    cg.add(var.set_max_still_distance(int(config[CONF_MAX_STILL_DISTANCE] / 0.75)))
    cg.add(
        var.set_range_config(
            config[CONF_G0_MOVE_THRESHOLD],
            config[CONF_G0_STILL_THRESHOLD],
            config[CONF_G1_MOVE_THRESHOLD],
            config[CONF_G1_STILL_THRESHOLD],
            config[CONF_G2_MOVE_THRESHOLD],
            config[CONF_G2_STILL_THRESHOLD],
            config[CONF_G3_MOVE_THRESHOLD],
            config[CONF_G3_STILL_THRESHOLD],
            config[CONF_G4_MOVE_THRESHOLD],
            config[CONF_G4_STILL_THRESHOLD],
            config[CONF_G5_MOVE_THRESHOLD],
            config[CONF_G5_STILL_THRESHOLD],
            config[CONF_G6_MOVE_THRESHOLD],
            config[CONF_G6_STILL_THRESHOLD],
            config[CONF_G7_MOVE_THRESHOLD],
            config[CONF_G7_STILL_THRESHOLD],
            config[CONF_G8_MOVE_THRESHOLD],
            config[CONF_G8_STILL_THRESHOLD],
        )
    )


CALIBRATION_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(LD2410Component),
    }
)
