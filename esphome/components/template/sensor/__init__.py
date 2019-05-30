import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_STATE, UNIT_EMPTY, ICON_EMPTY
from .. import template_ns

TemplateSensor = template_ns.class_('TemplateSensor', sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_EMPTY, ICON_EMPTY, 1).extend({
    cv.GenerateID(): cv.declare_id(TemplateSensor),
    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    if CONF_LAMBDA in config:
        template_ = yield cg.process_lambda(config[CONF_LAMBDA], [],
                                            return_type=cg.optional.template(float))
        cg.add(var.set_template(template_))


@automation.register_action('sensor.template.publish', sensor.SensorPublishAction,
                            cv.Schema({
                                cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
                                cv.Required(CONF_STATE): cv.templatable(cv.float_),
                            }))
def sensor_template_publish_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_STATE], args, float)
    cg.add(var.set_state(template_))
    yield var
