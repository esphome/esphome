import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor, i2c
from esphomeyaml.const import CONF_HUMIDITY, CONF_MAKE_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL, CONF_ID
from esphomeyaml.helpers import App, Application, variable, setup_component, PollingComponent, \
    Pvariable

DEPENDENCIES = ['i2c']

MakeHDC1080Sensor = Application.struct('MakeHDC1080Sensor')
HDC1080Component = sensor.sensor_ns.class_('HDC1080Component', PollingComponent, i2c.I2CDevice)
HDC1080TemperatureSensor = sensor.sensor_ns.class_('HDC1080TemperatureSensor',
                                                   sensor.EmptyPollingParentSensor)
HDC1080HumiditySensor = sensor.sensor_ns.class_('HDC1080HumiditySensor',
                                                sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeHDC1080Sensor),
    cv.GenerateID(): cv.declare_variable_id(HDC1080Component),
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HDC1080TemperatureSensor),
    })),
    vol.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HDC1080HumiditySensor),
    })),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_hdc1080_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                  config[CONF_HUMIDITY][CONF_NAME],
                                  config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    hdc1080 = make.Phdc1080
    Pvariable(config[CONF_ID], hdc1080)

    sensor.setup_sensor(hdc1080.Pget_temperature_sensor(),
                        make.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(hdc1080.Pget_humidity_sensor(), make.Pmqtt_humidity,
                        config[CONF_HUMIDITY])
    setup_component(hdc1080, config)


BUILD_FLAGS = '-DUSE_HDC1080_SENSOR'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_HUMIDITY])]
