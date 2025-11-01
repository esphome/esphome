import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_KEY, CONF_OPTIMISTIC, CONF_OPTIONS

from .. import CONF_CLUSTER, CONF_MIIO_ID, Miio, miio_ns

DEPENDENCIES = ["miio"]

MiioSelect = miio_ns.class_("MiioSelect", select.Select, cg.Component)


def ensure_option_map(value):
    cv.check_not_templatable(value)
    option = cv.All(cv.int_range(0, 2**8 - 1))
    mapping = cv.All(cv.string_strict)
    options_map_schema = cv.Schema({option: mapping})
    value = options_map_schema(value)

    all_values = list(value.keys())
    unique_values = set(value.keys())
    if len(all_values) != len(unique_values):
        raise cv.Invalid("Mapping values must be unique.")

    return value


CONFIG_SCHEMA = (
    select.select_schema(MiioSelect)
    .extend(
        {
            cv.GenerateID(CONF_MIIO_ID): cv.use_id(Miio),
            cv.Required(CONF_CLUSTER): cv.uint8_t,
            cv.Required(CONF_KEY): cv.uint8_t,
            cv.Required(CONF_OPTIONS): ensure_option_map,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    options = config[CONF_OPTIONS]
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await select.register_select(var, config, options=list(options.values()))

    parent = await cg.get_variable(config[CONF_MIIO_ID])

    cg.add(var.set_miio_parent(parent))
    cg.add(var.set_select_id(config[CONF_CLUSTER], config[CONF_KEY]))
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    cg.add(var.set_select_mappings(list(options.keys())))
