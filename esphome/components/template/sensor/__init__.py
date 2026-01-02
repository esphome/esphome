from esphome import automation
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_STATE

from .. import template_ns

TemplateSensor = template_ns.class_(
    "TemplateSensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        TemplateSensor,
        accuracy_decimals=1,
    )
    .extend(
        {
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(float)
        )
        cg.add(var.set_template(template_))


@automation.register_action(
    "sensor.template.publish",
    sensor.SensorPublishAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
            cv.Required(CONF_STATE): cv.templatable(cv.float_),
        }
    ),
)
async def sensor_template_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, float)
    cg.add(var.set_state(template_))
    return var
