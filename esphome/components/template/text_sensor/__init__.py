from esphome import automation
import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.text_sensor import TextSensorPublishAction
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_STATE

from .. import template_ns

TemplateTextSensor = template_ns.class_(
    "TemplateTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema()
    .extend(
        {
            cv.GenerateID(): cv.declare_id(TemplateTextSensor),
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.std_string)
        )
        cg.add(var.set_template(template_))


@automation.register_action(
    "text_sensor.template.publish",
    TextSensorPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(text_sensor.TextSensor),
            cv.Required(CONF_STATE): cv.templatable(cv.string_strict),
        }
    ),
)
async def text_sensor_template_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, cg.std_string)
    cg.add(var.set_state(template_))
    return var
