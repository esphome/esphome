from esphome import automation
import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_ID

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

DisplayOnAction = airton_ns.class_("DisplayOnAction", automation.Action)
DisplayOffAction = airton_ns.class_("DisplayOffAction", automation.Action)
SleepOnAction = airton_ns.class_("SleepOnAction", automation.Action)
SleepOffAction = airton_ns.class_("SleepOffAction", automation.Action)

AIRTON_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(AirtonClimate),
    }
)


@automation.register_action(
    "climate_ir.airton.display_on",
    DisplayOnAction,
    AIRTON_ACTION_SCHEMA,
    synchronous=False,
)
@automation.register_action(
    "climate_ir.airton.display_off",
    DisplayOffAction,
    AIRTON_ACTION_SCHEMA,
    synchronous=False,
)
async def display_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "climate_ir.airton.sleep_on", SleepOnAction, AIRTON_ACTION_SCHEMA, synchronous=False
)
@automation.register_action(
    "climate_ir.airton.sleep_off",
    SleepOffAction,
    AIRTON_ACTION_SCHEMA,
    synchronous=False,
)
async def sleep_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)
    cg.add(var.set_vertical_direction_state(config[CONF_VERTICAL_DIRECTION]))
