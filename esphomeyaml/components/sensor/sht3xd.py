import voluptuous as vol

from esphomeyaml.components import i2c, sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ADDRESS, CONF_HUMIDITY, CONF_MAKE_ID, CONF_NAME, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL, CONF_ID
from esphomeyaml.helpers import App, Application, PollingComponent, setup_component, variable, \
    Pvariable

DEPENDENCIES = ['i2c']

MakeSHT3XDSensor = Application.struct('MakeSHT3XDSensor')
SHT3XDComponent = sensor.sensor_ns.class_('SHT3XDComponent', PollingComponent, i2c.I2CDevice)
SHT3XDTemperatureSensor = sensor.sensor_ns.class_('SHT3XDTemperatureSensor',
                                                  sensor.EmptyPollingParentSensor)
SHT3XDHumiditySensor = sensor.sensor_ns.class_('SHT3XDHumiditySensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(SHT3XDComponent),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeSHT3XDSensor),
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(SHT3XDTemperatureSensor),
    })),
    vol.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(SHT3XDHumiditySensor),
    })),
    vol.Optional(CONF_ADDRESS, default=0x44): cv.i2c_address,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_sht3xd_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config[CONF_ADDRESS],
                                 config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    sht3xd = make.Psht3xd
    Pvariable(config[CONF_ID], sht3xd)

    sensor.setup_sensor(sht3xd.Pget_temperature_sensor(), make.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(sht3xd.Pget_humidity_sensor(), make.Pmqtt_humidity,
                        config[CONF_HUMIDITY])
    setup_component(sht3xd, config)


BUILD_FLAGS = '-DUSE_SHT3XD'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_HUMIDITY])]
