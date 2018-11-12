import voluptuous as vol

from esphomeyaml.components import i2c, sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_HUMIDITY, CONF_MAKE_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL, CONF_ID
from esphomeyaml.helpers import App, Application, PollingComponent, setup_component, variable, \
    Pvariable

DEPENDENCIES = ['i2c']

MakeHTU21DSensor = Application.struct('MakeHTU21DSensor')
HTU21DComponent = sensor.sensor_ns.class_('HTU21DComponent', PollingComponent, i2c.I2CDevice)
HTU21DTemperatureSensor = sensor.sensor_ns.class_('HTU21DTemperatureSensor',
                                                  sensor.EmptyPollingParentSensor)
HTU21DHumiditySensor = sensor.sensor_ns.class_('HTU21DHumiditySensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HTU21DComponent),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeHTU21DSensor),
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
    make = variable(config[CONF_MAKE_ID], rhs)
    htu21d = make.Phtu21d
    Pvariable(config[CONF_ID], htu21d)

    sensor.setup_sensor(htu21d.Pget_temperature_sensor(), make.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(htu21d.Pget_humidity_sensor(), make.Pmqtt_humidity,
                        config[CONF_HUMIDITY])
    setup_component(htu21d, config)


BUILD_FLAGS = '-DUSE_HTU21D_SENSOR'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_HUMIDITY])]
