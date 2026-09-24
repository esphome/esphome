from esphome import automation
import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_STATE

from .. import CONF_NEXTION_ID, CONF_PUBLISH_STATE, CONF_SEND_TO_NEXTION, nextion_ns
from ..base_component import CONFIG_TEXT_COMPONENT_SCHEMA, setup_component_core_

CODEOWNERS = ["@senexcrenshaw"]

NextionTextSensor = nextion_ns.class_(
    "NextionTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(NextionTextSensor)
    .extend(CONFIG_TEXT_COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("never"))
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NEXTION_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub)
    await text_sensor.register_text_sensor(var, config)
    await cg.register_component(var, config)

    cg.add(hub.register_textsensor_component(var))

    await setup_component_core_(var, config, ".txt")


automation.register_apply_action(
    "text_sensor.nextion.publish",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(NextionTextSensor),
            cv.Required(CONF_STATE): cv.templatable(cv.string_strict),
            cv.Optional(CONF_PUBLISH_STATE, default="true"): cv.templatable(cv.boolean),
            cv.Optional(CONF_SEND_TO_NEXTION, default="true"): cv.templatable(
                cv.boolean
            ),
        }
    ),
    automation.ApplyCall(
        "set_state({}, {}, {})",
        (
            (CONF_STATE, cg.std_string),
            (CONF_PUBLISH_STATE, cg.bool_),
            (CONF_SEND_TO_NEXTION, cg.bool_),
        ),
    ),
)
