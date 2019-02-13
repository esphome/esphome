import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Application, PollingComponent

DEPENDENCIES = ['i2c']

MakeHTU21DSensor = Application.struct('MakeHTU21DSensor')
HTU21DComponent = sensor.sensor_ns.class_('HTU21DComponent', PollingComponent, i2c.I2CDevice)
HTU21DTemperatureSensor = sensor.sensor_ns.class_('HTU21DTemperatureSensor',
                                                  sensor.EmptyPollingParentSensor)
HTU21DHumiditySensor = sensor.sensor_ns.class_('HTU21DHumiditySensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HTU21DComponent),
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HTU21DTemperatureSensor),
    })),
    vol.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HTU21DHumiditySensor),
    })),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_htu21d_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config.get(CONF_UPDATE_INTERVAL))
    htu21d = Pvariable(config[CONF_ID], rhs)

    sensor.setup_sensor(htu21d.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_sensor(htu21d.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    setup_component(htu21d, config)


BUILD_FLAGS = '-DUSE_HTU21D_SENSOR'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_HUMIDITY])]
