import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, spi
from esphome.const import CONF_ID, ICON_THERMOMETER, UNIT_CELSIUS

DEPENDENCIES = ['spi']

CONF_EVEN_PIN_COUNT = 'even_pin_count'
CONF_R_NOMINAL = 'r_nominal'
CONF_R_REF = 'r_ref'
CONF_FILTER_50HZ = 'filter_50hz'

max31865_ns = cg.esphome_ns.namespace('max31865')
MAX31865Sensor = max31865_ns.class_('MAX31865Sensor', sensor.Sensor, cg.PollingComponent,
                                    spi.SPIDevice)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1).extend({
    cv.GenerateID(): cv.declare_id(MAX31865Sensor),
    cv.Optional(CONF_EVEN_PIN_COUNT, default=False): cv.boolean,
    cv.Optional(CONF_R_NOMINAL, default=1000.0): cv.float_,
    cv.Optional(CONF_R_REF, default=4300.0): cv.float_,
    cv.Optional(CONF_FILTER_50HZ, default=False): cv.boolean,
}).extend(cv.polling_component_schema('60s')).extend(spi.SPI_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    cg.add(var.set_pin_count(config[CONF_EVEN_PIN_COUNT]))
    cg.add(var.set_r_nominal(config[CONF_R_NOMINAL]))
    cg.add(var.set_r_ref(config[CONF_R_REF]))
    cg.add(var.set_filter_50hz(config[CONF_FILTER_50HZ]))
    yield spi.register_spi_device(var, config)
    yield sensor.register_sensor(var, config)
