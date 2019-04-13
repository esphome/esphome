from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_SENSORS
CustomSensorConstructor = sensor.sensor_ns.class_('CustomSensorConstructor')

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(CustomSensorConstructor),
    cv.Required(CONF_LAMBDA): cv.lambda_,
    cv.Required(CONF_SENSORS): cv.ensure_list(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(sensor.Sensor),
    })),
})


def to_code(config):
    template_ = yield process_lambda(config[CONF_LAMBDA], [],
                                     return_type=std_vector.template(sensor.SensorPtr))

    rhs = CustomSensorConstructor(template_)
    custom = variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_SENSORS]):
        rhs = custom.Pget_sensor(i)
        cg.add(rhs.set_name(conf[CONF_NAME]))
        sensor.register_sensor(rhs, conf)


BUILD_FLAGS = '-DUSE_CUSTOM_SENSOR'
