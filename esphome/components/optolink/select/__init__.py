import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import (
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_FROM,
    CONF_ID,
    CONF_TO,
)
from .. import optolink_ns, CONF_OPTOLINK_ID, SENSOR_BASE_SCHEMA

DEPENDENCIES = ["optolink"]
CODEOWNERS = ["@j0ta29"]


OptolinkSelect = optolink_ns.class_(
    "OptolinkSelect", select.Select, cg.PollingComponent
)


def validate_mapping(value):
    if not isinstance(value, dict):
        value = cv.string(value)
        if "->" not in value:
            raise cv.Invalid("Mapping must contain '->'")
        a, b = value.split("->", 1)
        value = {CONF_FROM: a.strip(), CONF_TO: b.strip()}

    return cv.Schema(
        {cv.Required(CONF_FROM): cv.string, cv.Required(CONF_TO): cv.string}
    )(value)


CONF_MAP = "map"
MAP_ID = "mappings"
CONFIG_SCHEMA = (
    select.SELECT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(OptolinkSelect),
            cv.GenerateID(MAP_ID): cv.declare_id(
                cg.std_ns.class_("map").template(cg.std_string, cg.std_string)
            ),
            cv.Required(CONF_MAP): cv.ensure_list(validate_mapping),
            cv.Required(CONF_ADDRESS): cv.hex_uint32_t,
            cv.Required(CONF_BYTES): cv.one_of(1, 2, 4, int=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(SENSOR_BASE_SCHEMA)
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_OPTOLINK_ID])
    var = cg.new_Pvariable(config[CONF_ID], component)

    await cg.register_component(var, config)
    await select.register_select(
        var,
        config,
        options=[],
    )

    map_type_ = cg.std_ns.class_("map").template(cg.std_string, cg.std_string)
    map_var = cg.new_Pvariable(
        config[MAP_ID],
        map_type_([(item[CONF_FROM], item[CONF_TO]) for item in config[CONF_MAP]]),
    )

    cg.add(var.set_map(map_var))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_bytes(config[CONF_BYTES]))
    cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
