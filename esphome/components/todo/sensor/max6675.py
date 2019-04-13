from esphome import pins
from esphome.components import sensor, spi
from esphome.components.spi import SPIComponent
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_CS_PIN, CONF_ID, CONF_NAME, CONF_SPI_ID, \
    CONF_UPDATE_INTERVAL


MAX6675Sensor = sensor.sensor_ns.class_('MAX6675Sensor', sensor.PollingSensorComponent,
                                        spi.SPIDevice)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MAX6675Sensor),
    cv.GenerateID(CONF_SPI_ID): cv.use_variable_id(SPIComponent),
    cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    spi_ = yield get_variable(config[CONF_SPI_ID])
    cs = yield gpio_output_pin_expression(config[CONF_CS_PIN])
    rhs = App.make_max6675_sensor(config[CONF_NAME], spi_, cs,
                                  config.get(CONF_UPDATE_INTERVAL))
    max6675 = Pvariable(config[CONF_ID], rhs)
    sensor.setup_sensor(max6675, config)
    register_component(max6675, config)


BUILD_FLAGS = '-DUSE_MAX6675_SENSOR'
