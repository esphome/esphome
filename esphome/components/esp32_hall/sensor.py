from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NAME, CONF_UPDATE_INTERVAL, ESP_PLATFORM_ESP32, \
    CONF_UNIT_OF_MEASUREMENT, CONF_ICON, CONF_ACCURACY_DECIMALS, UNIT_MICROTESLA, ICON_MAGNET

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

esp32_hall_ns = cg.esphome_ns.namespace('esp32_hall')
ESP32HallSensor = esp32_hall_ns.class_('ESP32HallSensor', sensor.PollingSensorComponent)

CONFIG_SCHEMA = cv.nameable(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(ESP32HallSensor),
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,

    cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_MICROTESLA): sensor.unit_of_measurement,
    cv.Optional(CONF_ICON, default=ICON_MAGNET): sensor.icon,
    cv.Optional(CONF_ACCURACY_DECIMALS, default=-1): sensor.accuracy_decimals,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], config[CONF_UPDATE_INTERVAL])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
