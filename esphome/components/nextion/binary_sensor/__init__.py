import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from esphome.const import CONF_COMPONENT_ID, CONF_PAGE_ID, CONF_ID
from .. import nextion_ns, CONF_NEXTION_ID


from ..base_component import (
    setup_component_core_,
    CONFIG_BINARY_SENSOR_SCHEMA,
    CONF_VARIABLE_NAME,
    CONF_COMPONENT_NAME,
)

CODEOWNERS = ["@senexcrenshaw"]

NextionBinarySensor = nextion_ns.class_(
    "NextionBinarySensor", binary_sensor.BinarySensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(NextionBinarySensor)
    .extend(
        {
            cv.Optional(CONF_PAGE_ID): cv.uint8_t,
            cv.Optional(CONF_COMPONENT_ID): cv.uint8_t,
        }
    )
    .extend(CONFIG_BINARY_SENSOR_SCHEMA)
    .extend(cv.polling_component_schema("never")),
    cv.has_at_least_one_key(
        CONF_PAGE_ID,
        CONF_COMPONENT_ID,
        CONF_COMPONENT_NAME,
        CONF_VARIABLE_NAME,
    ),
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NEXTION_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub)
    await binary_sensor.register_binary_sensor(var, config)
    await cg.register_component(var, config)

    if config.keys() >= {CONF_PAGE_ID, CONF_COMPONENT_ID}:
        cg.add(hub.register_touch_component(var))
        cg.add(var.set_component_id(config[CONF_COMPONENT_ID]))
        cg.add(var.set_page_id(config[CONF_PAGE_ID]))

    if CONF_COMPONENT_NAME in config or CONF_VARIABLE_NAME in config:
        await setup_component_core_(var, config, ".val")
        cg.add(hub.register_binarysensor_component(var))
