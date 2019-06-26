import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, spi
from esphome.const import CONF_ID, ICON_THERMOMETER, UNIT_CELSIUS

max31865_ns = cg.esphome_ns.namespace('max31865')
MAX31865Sensor = max31865_ns.class_('MAX31865Sensor', sensor.Sensor, cg.PollingComponent,
                                    spi.SPIDevice)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1).extend({
    cv.GenerateID(): cv.declare_id(MAX31865Sensor),
}).extend(cv.polling_component_schema('60s')).extend(spi.SPI_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)
    yield sensor.register_sensor(var, config)
