from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NAME, CONF_UPDATE_INTERVAL, ESP_PLATFORM_ESP32


ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

MakeESP32HallSensor = Application.struct('MakeESP32HallSensor')
ESP32HallSensor = sensor.sensor_ns.class_('ESP32HallSensor', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(ESP32HallSensor),
    cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    rhs = App.make_esp32_hall_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    hall = Pvariable(config[CONF_ID], rhs)
    sensor.setup_sensor(hall, config)
    register_component(hall, config)


BUILD_FLAGS = '-DUSE_ESP32_HALL_SENSOR'
