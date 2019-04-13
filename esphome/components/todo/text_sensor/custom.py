from esphome.components import text_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_TEXT_SENSORS
CustomTextSensorConstructor = text_sensor.text_sensor_ns.class_('CustomTextSensorConstructor')

PLATFORM_SCHEMA = text_sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(CustomTextSensorConstructor),
    cv.Required(CONF_LAMBDA): cv.lambda_,
    cv.Required(CONF_TEXT_SENSORS):
        cv.ensure_list(text_sensor.TEXT_SENSOR_SCHEMA.extend({
            cv.GenerateID(): cv.declare_variable_id(text_sensor.TextSensor),
        })),
})


def to_code(config):
    template_ = yield process_lambda(config[CONF_LAMBDA], [],
                                     return_type=std_vector.template(text_sensor.TextSensorPtr))

    rhs = CustomTextSensorConstructor(template_)
    custom = variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_TEXT_SENSORS]):
        rhs = custom.Pget_text_sensor(i)
        cg.add(rhs.set_name(conf[CONF_NAME]))
        text_sensor.register_text_sensor(rhs, conf)


BUILD_FLAGS = '-DUSE_CUSTOM_TEXT_SENSOR'
