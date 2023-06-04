import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

CODEOWNERS = ["@descipher"]

DEPENDENCIES = ["uart"]

MULTI_CONF = True

ld2420_ns = cg.esphome_ns.namespace("ld2420")
LD2420Component = ld2420_ns.class_("LD2420Component", cg.Component, uart.UARTDevice)

CONF_LD2420_ID = "ld2420_id"
CONF_DETECTION_GATE_MIN = "detection_gate_min"
CONF_DETECTION_GATE_MAX = "detection_gate_max"
CONF_PRESENCE_TIME_WINDOW = "presence_time_window"
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
CONF_G9_MOVE_THRESHOLD = "g9_move_threshold"
CONF_G9_STILL_THRESHOLD = "g9_still_threshold"
CONF_G10_MOVE_THRESHOLD = "g10_move_threshold"
CONF_G10_STILL_THRESHOLD = "g10_still_threshold"
CONF_G11_MOVE_THRESHOLD = "g11_move_threshold"
CONF_G11_STILL_THRESHOLD = "g11_still_threshold"
CONF_G12_MOVE_THRESHOLD = "g12_move_threshold"
CONF_G12_STILL_THRESHOLD = "g12_still_threshold"
CONF_G13_MOVE_THRESHOLD = "g13_move_threshold"
CONF_G13_STILL_THRESHOLD = "g13_still_threshold"
CONF_G14_MOVE_THRESHOLD = "g14_move_threshold"
CONF_G14_STILL_THRESHOLD = "g14_still_threshold"
CONF_G15_MOVE_THRESHOLD = "g15_move_threshold"
CONF_G15_STILL_THRESHOLD = "g15_still_threshold"

GATES = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2420Component),
            cv.Optional(CONF_DETECTION_GATE_MAX, default=12): cv.int_range(
                min=0, max=15
            ),
            cv.Optional(CONF_DETECTION_GATE_MIN, default=1): cv.int_range(
                min=0, max=15
            ),
            cv.Optional(CONF_PRESENCE_TIME_WINDOW, default="120s"): cv.All(
                cv.positive_time_period_seconds,
                cv.Range(max=cv.TimePeriod(seconds=32767)),
            ),
            cv.Optional(CONF_G0_MOVE_THRESHOLD, default=60000): cv.uint16_t,
            cv.Optional(CONF_G0_STILL_THRESHOLD, default=40000): cv.uint16_t,
            cv.Optional(CONF_G1_MOVE_THRESHOLD, default=30000): cv.uint16_t,
            cv.Optional(CONF_G1_STILL_THRESHOLD, default=20000): cv.uint16_t,
            cv.Optional(CONF_G2_MOVE_THRESHOLD, default=400): cv.uint16_t,
            cv.Optional(CONF_G2_STILL_THRESHOLD, default=200): cv.uint16_t,
            cv.Optional(CONF_G3_MOVE_THRESHOLD, default=250): cv.uint16_t,
            cv.Optional(CONF_G3_STILL_THRESHOLD, default=200): cv.uint16_t,
            cv.Optional(CONF_G4_MOVE_THRESHOLD, default=250): cv.uint16_t,
            cv.Optional(CONF_G4_STILL_THRESHOLD, default=200): cv.uint16_t,
            cv.Optional(CONF_G5_MOVE_THRESHOLD, default=250): cv.uint16_t,
            cv.Optional(CONF_G5_STILL_THRESHOLD, default=200): cv.uint16_t,
            cv.Optional(CONF_G6_MOVE_THRESHOLD, default=250): cv.uint16_t,
            cv.Optional(CONF_G6_STILL_THRESHOLD, default=200): cv.uint16_t,
            cv.Optional(CONF_G7_MOVE_THRESHOLD, default=250): cv.uint16_t,
            cv.Optional(CONF_G7_STILL_THRESHOLD, default=200): cv.uint16_t,
            cv.Optional(CONF_G8_MOVE_THRESHOLD, default=250): cv.uint16_t,
            cv.Optional(CONF_G8_STILL_THRESHOLD, default=150): cv.uint16_t,
            cv.Optional(CONF_G9_MOVE_THRESHOLD, default=250): cv.uint16_t,
            cv.Optional(CONF_G9_STILL_THRESHOLD, default=150): cv.uint16_t,
            cv.Optional(CONF_G10_MOVE_THRESHOLD, default=250): cv.uint16_t,
            cv.Optional(CONF_G10_STILL_THRESHOLD, default=100): cv.uint16_t,
            cv.Optional(CONF_G11_MOVE_THRESHOLD, default=250): cv.uint16_t,
            cv.Optional(CONF_G11_STILL_THRESHOLD, default=100): cv.uint16_t,
            cv.Optional(CONF_G12_MOVE_THRESHOLD, default=250): cv.uint16_t,
            cv.Optional(CONF_G12_STILL_THRESHOLD, default=100): cv.uint16_t,
            cv.Optional(CONF_G13_MOVE_THRESHOLD, default=200): cv.uint16_t,
            cv.Optional(CONF_G13_STILL_THRESHOLD, default=100): cv.uint16_t,
            cv.Optional(CONF_G14_MOVE_THRESHOLD, default=200): cv.uint16_t,
            cv.Optional(CONF_G14_STILL_THRESHOLD, default=100): cv.uint16_t,
            cv.Optional(CONF_G15_MOVE_THRESHOLD, default=200): cv.uint16_t,
            cv.Optional(CONF_G15_STILL_THRESHOLD, default=100): cv.uint16_t,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ld2420_uart",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_timeout(config[CONF_PRESENCE_TIME_WINDOW]))
    cg.add(var.set_min_gate(config[CONF_DETECTION_GATE_MIN]))
    cg.add(var.set_max_gate(config[CONF_DETECTION_GATE_MAX]))
    cg.add(
        var.set_gate_sense_config(
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
            config[CONF_G9_MOVE_THRESHOLD],
            config[CONF_G9_STILL_THRESHOLD],
            config[CONF_G10_MOVE_THRESHOLD],
            config[CONF_G10_STILL_THRESHOLD],
            config[CONF_G11_MOVE_THRESHOLD],
            config[CONF_G11_STILL_THRESHOLD],
            config[CONF_G12_MOVE_THRESHOLD],
            config[CONF_G12_STILL_THRESHOLD],
            config[CONF_G13_MOVE_THRESHOLD],
            config[CONF_G13_STILL_THRESHOLD],
            config[CONF_G14_MOVE_THRESHOLD],
            config[CONF_G14_STILL_THRESHOLD],
            config[CONF_G15_MOVE_THRESHOLD],
            config[CONF_G15_STILL_THRESHOLD],
        )
    )
