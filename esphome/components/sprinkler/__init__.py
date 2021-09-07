import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    CONF_RUN_DURATION,
)

CODEOWNERS = ["@kbx81"]

CONF_PUMP = "pump"
CONF_NAME = "name"
CONF_ENABLE_SWITCH = "enable_switch"
CONF_VALVE_SWITCH = "valve_switch"
CONF_VALVE_OPEN_DELAY = "valve_open_delay"
CONF_VALVES = "valves"

sprinkler_ns = cg.esphome_ns.namespace("sprinkler")
Sprinkler = sprinkler_ns.class_("Sprinkler")


def validate_sprinkler(config):
    for valve in config[CONF_VALVES]:
        if valve[CONF_RUN_DURATION] <= config[CONF_VALVE_OPEN_DELAY]:
            raise cv.Invalid(
                f"{CONF_RUN_DURATION} must be greater than {CONF_VALVE_OPEN_DELAY}"
            )
    return config


SPRINKLER_VALVE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_RUN_DURATION): cv.positive_time_period_seconds,
        cv.Required(CONF_VALVE_SWITCH): cv.use_id(switch.Switch),
        cv.Optional(CONF_ENABLE_SWITCH): cv.use_id(switch.Switch),
        cv.Optional(CONF_NAME): cv.string,
    }
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Sprinkler),
            cv.Optional(CONF_PUMP): cv.use_id(switch.Switch),
            cv.Required(CONF_VALVE_OPEN_DELAY): cv.positive_time_period_seconds,
            cv.Required(CONF_VALVES): cv.ensure_list(SPRINKLER_VALVE_SCHEMA),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_sprinkler,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    # await cg.register_component(var, config)
    # await climate.register_climate(var, config)

    cg.add(var.set_valve_open_delay(config[CONF_VALVE_OPEN_DELAY]))

    if CONF_PUMP in config:
        pump = await cg.get_variable(config[CONF_PUMP])
        cg.add(var.set_pump_switch(pump))

    for valve in config[CONF_VALVES]:
        valve_switch = await cg.get_variable(valve[CONF_VALVE_SWITCH])
        if CONF_ENABLE_SWITCH in valve:
            enable_switch = await cg.get_variable(valve[CONF_ENABLE_SWITCH])
            cg.add(
                var.add_valve(
                    valve_switch,
                    enable_switch,
                    valve[CONF_RUN_DURATION],
                    valve[CONF_NAME],
                )
            )
        else:
            cg.add(
                var.add_valve(valve_switch, valve[CONF_RUN_DURATION], valve[CONF_NAME])
            )
