import esphome.codegen as cg
from esphome.components import groups, one_wire
import esphome.config_validation as cv
from esphome.const import CONF_GROUPS, CONF_ID, CONF_MODE, CONF_UPDATE_INTERVAL

AUTO_LOAD = ["dallas_temp"]

dallas_temp_searcher_ns = cg.esphome_ns.namespace("dallas_temp_searcher")
DallasTempSearcherComponent = dallas_temp_searcher_ns.class_(
    "DallasTemperatureSearcher", cg.Component
)

CONF_MAX_SENSORS_NUM = "max_sensors_num"

search_mode = dallas_temp_searcher_ns.enum("SearchMode", is_class=True)

SEARCH_MODE = {
    "all": search_mode.ALL,
    "address_map": search_mode.ADDRESS_MAP,
}

MAX_SENSOR_NUM_SCHEMA = cv.All(
    cv.Schema(
        {cv.Required(CONF_MAX_SENSORS_NUM): cv.int_range(0, 32)}, extra=cv.ALLOW_EXTRA
    )
)


def check_mode(config):
    if CONF_MODE in config:
        if config[CONF_MODE] == "address_map":
            MAX_SENSOR_NUM_SCHEMA(config)
        elif config[CONF_MODE] == "all":
            if CONF_MAX_SENSORS_NUM in config:
                raise cv.Invalid(
                    f"{CONF_MAX_SENSORS_NUM} param is needed only in address_map mode"
                )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DallasTempSearcherComponent),
            cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.update_interval,
            cv.Optional(CONF_MODE, default="all"): cv.enum(SEARCH_MODE, lower=True),
            cv.Optional(CONF_MAX_SENSORS_NUM): cv.int_,
        }
    ).extend(one_wire.one_wire_device_schema().extend(groups.LIST_OF_GROUPS_SCHEMA)),
    check_mode,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await one_wire.register_one_wire_device(var, config)

    if CONF_MAX_SENSORS_NUM in config:
        cg.add(var.set_max_sensors_num(config[CONF_MAX_SENSORS_NUM]))
    cg.add(var.set_search_mode(config[CONF_MODE]))

    if group_config := config.get(CONF_GROUPS):
        await groups.add_groups_to_storage(var, group_config)
