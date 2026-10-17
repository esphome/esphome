from esphome import automation
import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_DEVICE_CLASS, CONF_ID, CONF_LAMBDA, CONF_STATE

from .. import template_ns

TemplateTextSensor = template_ns.class_(
    "TemplateTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    cv.with_visibility(
        text_sensor.text_sensor_schema(),
        cv.Visibility.UI,
        CONF_DEVICE_CLASS,
    )
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


automation.register_apply_action(
    "text_sensor.template.publish",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(text_sensor.TextSensor),
            cv.Required(CONF_STATE): cv.templatable(cv.string_strict),
        }
    ),
    automation.ApplyField(CONF_STATE, "publish_state", cg.std_string),
)
