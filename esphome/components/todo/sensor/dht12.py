from esphome.components import i2c, sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL


DEPENDENCIES = ['i2c']

DHT12Component = sensor.sensor_ns.class_('DHT12Component', PollingComponent, i2c.I2CDevice)
DHT12TemperatureSensor = sensor.sensor_ns.class_('DHT12TemperatureSensor',
                                                 sensor.EmptyPollingParentSensor)
DHT12HumiditySensor = sensor.sensor_ns.class_('DHT12HumiditySensor',
                                              sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(DHT12Component),
    cv.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(DHT12TemperatureSensor),
    })),
    cv.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(DHT12HumiditySensor),
    })),
    cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = App.make_dht12_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                config[CONF_HUMIDITY][CONF_NAME],
                                config.get(CONF_UPDATE_INTERVAL))
    dht = Pvariable(config[CONF_ID], rhs)

    sensor.setup_sensor(dht.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_sensor(dht.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    register_component(dht, config)


BUILD_FLAGS = '-DUSE_DHT12_SENSOR'
