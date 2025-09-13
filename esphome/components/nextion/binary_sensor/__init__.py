from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_COMPONENT_ID, CONF_ID, CONF_PAGE_ID, CONF_STATE

from .. import CONF_NEXTION_ID, CONF_PUBLISH_STATE, CONF_SEND_TO_NEXTION, nextion_ns
from ..base_component import (
    CONF_COMPONENT_NAME,
    CONF_VARIABLE_NAME,
    CONFIG_BINARY_SENSOR_SCHEMA,
    setup_component_core_,
)

CODEOWNERS = ["@senexcrenshaw"]

NextionBinarySensor = nextion_ns.class_(
    "NextionBinarySensor", binary_sensor.BinarySensor, cg.PollingComponent
)

NextionPublishBoolAction = nextion_ns.class_(
    "NextionPublishBoolAction", automation.Action
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


@automation.register_action(
    "binary_sensor.nextion.publish",
    NextionPublishBoolAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(NextionBinarySensor),
            cv.Required(CONF_STATE): cv.templatable(cv.boolean),
            cv.Optional(CONF_PUBLISH_STATE, default="true"): cv.templatable(cv.boolean),
            cv.Optional(CONF_SEND_TO_NEXTION, default="true"): cv.templatable(
                cv.boolean
            ),
        }
    ),
)
async def sensor_nextion_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))

    template_ = await cg.templatable(config[CONF_PUBLISH_STATE], args, bool)
    cg.add(var.set_publish_state(template_))

    template_ = await cg.templatable(config[CONF_SEND_TO_NEXTION], args, bool)
    cg.add(var.set_send_to_nextion(template_))

    return var
