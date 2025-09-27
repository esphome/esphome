import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor, text_sensor
from esphome.const import (
    CONF_DYNAMIC_LAMBDA,
    CONF_ID,
    CONF_LAMBDA,
    CONF_SOURCE_ID,
    CONF_STATE,
)
from .. import template_ns

TemplateSensor = template_ns.class_(
    "TemplateSensor", sensor.Sensor, cg.PollingComponent
)


def _validate(config):
    if CONF_LAMBDA in config and CONF_DYNAMIC_LAMBDA in config:
        raise cv.Invalid("dynamic_lambda cannot be used with lambda")
    return config

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        TemplateSensor,
        accuracy_decimals=1,
    )
    .extend(
        {
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_DYNAMIC_LAMBDA): cv.Schema(
                {
                    cv.Required(CONF_SOURCE_ID): cv.use_id(text_sensor.TextSensor),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s")),
    _validate,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(float)
        )
        cg.add(var.set_template(template_))

    if CONF_DYNAMIC_LAMBDA in config:
        dyn = config[CONF_DYNAMIC_LAMBDA]
        cg.add_define("USE_TEMPLATE_SENSOR_DYNAMIC_LAMBDA")
        cg.add(var.set_dynamic(True))
        source = await cg.get_variable(dyn[CONF_SOURCE_ID])
        cg.add(var.set_lambda_source(source))
        cg.add_library("jingoro2112/wrench", "6.0.18")


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
