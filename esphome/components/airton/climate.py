from esphome import automation
import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv

AUTO_LOAD = ["climate_ir"]

airton_ns = cg.esphome_ns.namespace("airton")
AirtonClimate = airton_ns.class_("AirtonClimate", climate_ir.ClimateIR)

CONF_AIRTON_ID = "airton_id"
CONF_SLEEP_MODE = "sleep_mode"
CONF_VERTICAL_DIRECTION = "vertical_direction"

VerticalDirections = airton_ns.enum("VerticalDirections")
VERTICAL_DIRECTIONS = {
    "off": VerticalDirections.VERTICAL_DIRECTION_OFF,
    "swing": VerticalDirections.VERTICAL_DIRECTION_SWING,
    "up": VerticalDirections.VERTICAL_DIRECTION_UP,
    "middle-up": VerticalDirections.VERTICAL_DIRECTION_MIDDLE_UP,
    "middle": VerticalDirections.VERTICAL_DIRECTION_MIDDLE,
    "middle-down": VerticalDirections.VERTICAL_DIRECTION_MIDDLE_DOWN,
    "down": VerticalDirections.VERTICAL_DIRECTION_DOWN,
}

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(AirtonClimate).extend(
    {
        cv.Optional(CONF_VERTICAL_DIRECTION, default="off"): cv.enum(
            VERTICAL_DIRECTIONS
        ),
    }
)

AIRTON_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(AirtonClimate),
    }
)

for _name, _call in (
    ("climate_ir.airton.display_on", "set_display_state(true, true)"),
    ("climate_ir.airton.display_off", "set_display_state(false, true)"),
    ("climate_ir.airton.sleep_on", "set_sleep_mode_state(true, true)"),
    ("climate_ir.airton.sleep_off", "set_sleep_mode_state(false, true)"),
):
    automation.register_apply_action(
        _name, AIRTON_ACTION_SCHEMA, automation.ApplyCall(_call)
    )


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_vertical_direction_state(config[CONF_VERTICAL_DIRECTION]))
