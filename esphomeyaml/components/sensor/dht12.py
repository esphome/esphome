import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor, i2c
from esphomeyaml.const import CONF_HUMIDITY, CONF_MAKE_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL, CONF_ID
from esphomeyaml.helpers import App, Application, variable, setup_component, PollingComponent, \
    Pvariable

DEPENDENCIES = ['i2c']

MakeDHT12Sensor = Application.struct('MakeDHT12Sensor')
DHT12Component = sensor.sensor_ns.class_('DHT12Component', PollingComponent, i2c.I2CDevice)
DHT12TemperatureSensor = sensor.sensor_ns.class_('DHT12TemperatureSensor',
                                                 sensor.EmptyPollingParentSensor)
DHT12HumiditySensor = sensor.sensor_ns.class_('DHT12HumiditySensor',
                                              sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeDHT12Sensor),
    cv.GenerateID(): cv.declare_variable_id(DHT12Component),
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(DHT12TemperatureSensor),
    })),
    vol.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(DHT12HumiditySensor),
    })),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_dht12_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                config[CONF_HUMIDITY][CONF_NAME],
                                config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    dht = make.Pdht12
    Pvariable(config[CONF_ID], dht)

    sensor.setup_sensor(dht.Pget_temperature_sensor(), make.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(dht.Pget_humidity_sensor(), make.Pmqtt_humidity,
                        config[CONF_HUMIDITY])
    setup_component(dht, config)


BUILD_FLAGS = '-DUSE_DHT12_SENSOR'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_HUMIDITY])]
