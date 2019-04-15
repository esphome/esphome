from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_SENSORS
from .. import custom_ns

CustomSensorConstructor = custom_ns.class_('CustomSensorConstructor')

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(CustomSensorConstructor),
    cv.Required(CONF_LAMBDA): cv.lambda_,
    cv.Required(CONF_SENSORS): cv.ensure_list(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(sensor.Sensor),
    })),
})


def to_code(config):
    template_ = yield cg.process_lambda(
        config[CONF_LAMBDA], [], return_type=cg.std_vector.template(sensor.SensorPtr))

    rhs = CustomSensorConstructor(template_)
    var = cg.variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_SENSORS]):
        sens = cg.new_Pvariable(conf[CONF_ID], var.get_switch(i))
        cg.add(sens.set_name(conf[CONF_NAME]))
        yield sensor.register_sensor(sens, conf)
