import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, ESP_PLATFORM_ESP32, UNIT_MICROTESLA, ICON_MAGNET

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

esp32_hall_ns = cg.esphome_ns.namespace('esp32_hall')
ESP32HallSensor = esp32_hall_ns.class_('ESP32HallSensor', sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_MICROTESLA, ICON_MAGNET, 1).extend({
    cv.GenerateID(): cv.declare_id(ESP32HallSensor),
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
