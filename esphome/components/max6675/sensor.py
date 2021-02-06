import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, spi
from esphome.const import CONF_ID, ICON_THERMOMETER, UNIT_CELSIUS

max6675_ns = cg.esphome_ns.namespace('max6675')
MAX6675Sensor = max6675_ns.class_('MAX6675Sensor', sensor.Sensor, cg.PollingComponent,
                                  spi.SPIDevice)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1).extend({
    cv.GenerateID(): cv.declare_id(MAX6675Sensor),
}).extend(cv.polling_component_schema('60s')).extend(spi.spi_device_schema())


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)
    yield sensor.register_sensor(var, config)
