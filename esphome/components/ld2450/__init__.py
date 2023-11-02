import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_THROTTLE

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@amcfague"]

ld2450_ns = cg.esphome_ns.namespace("ld2450")
LD2450Component = ld2450_ns.class_("LD2450Component", cg.Component, uart.UARTDevice)
PresenceZone = ld2450_ns.class_("PresenceZone")

CONF_LD2450_ID = "ld2450_id"
CONF_PRESENCE = "presence"
CONF_TARGET_COUNT = "target_count"
CONF_X_START = "x_start"
CONF_Y_START = "y_start"
CONF_X_END = "x_end"
CONF_Y_END = "y_end"
CONF_ZONES = "zones"
CONF_ZONE_ID = "zone_id"

# This is currently hard-coded from the sensor itself, but it could change one
# day, so define it here. It is also defined in the C++ code via code gen.
NUM_TARGETS = 3


def verify_coordinates(zone):
    id = zone[CONF_ID]
    x_start = zone[CONF_X_START]
    y_start = zone[CONF_Y_START]
    x_end = zone[CONF_X_END]
    y_end = zone[CONF_Y_END]

    errors = []
    if x_start > x_end:
        errors.append(f"{id}: x_end ({x_end}) must be less than x_start ({x_start})")
    if y_start > y_end:
        errors.append(f"{id}: y_end ({y_end}) must be less than y_start ({y_start})")

    if errors:
        raise cv.MultipleInvalid(errors=errors)

    return zone


coordinate_ranges = cv.int_range(-4000, 4000)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LD2450Component),
        cv.Optional(CONF_THROTTLE, default="1000ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=1)),
        ),
        cv.Optional(CONF_ZONES): cv.ensure_list(
            cv.All(
                cv.Schema(
                    {
                        cv.GenerateID(): cv.declare_id(PresenceZone),
                        cv.Required(CONF_X_START): coordinate_ranges,
                        cv.Required(CONF_Y_START): coordinate_ranges,
                        cv.Required(CONF_X_END): coordinate_ranges,
                        cv.Required(CONF_Y_END): coordinate_ranges,
                    }
                ),
                verify_coordinates,
            )
        ),
    }
)

CONFIG_SCHEMA = cv.All(
    CONFIG_SCHEMA.extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ld2450",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add_define("NUM_TARGETS", NUM_TARGETS)
    cg.add(var.set_throttle(config[CONF_THROTTLE]))
    if zones := config.get(CONF_ZONES):
        for zone in zones:
            zone_obj = cg.new_Pvariable(
                zone[CONF_ID],
                zone[CONF_X_START],
                zone[CONF_Y_START],
                zone[CONF_X_END],
                zone[CONF_Y_END],
            )
            cg.add(var.add_zone(zone_obj))
