from esphome.components import i2c, sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_HUMIDITY, CONF_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL


DEPENDENCIES = ['i2c']

MakeHTU21DSensor = Application.struct('MakeHTU21DSensor')
HTU21DComponent = sensor.sensor_ns.class_('HTU21DComponent', PollingComponent, i2c.I2CDevice)
HTU21DTemperatureSensor = sensor.sensor_ns.class_('HTU21DTemperatureSensor',
                                                  sensor.EmptyPollingParentSensor)
HTU21DHumiditySensor = sensor.sensor_ns.class_('HTU21DHumiditySensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HTU21DComponent),
    cv.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HTU21DTemperatureSensor),
    })),
    cv.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HTU21DHumiditySensor),
    })),
    cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = App.make_htu21d_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config.get(CONF_UPDATE_INTERVAL))
    htu21d = Pvariable(config[CONF_ID], rhs)

    sensor.setup_sensor(htu21d.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_sensor(htu21d.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    register_component(htu21d, config)


BUILD_FLAGS = '-DUSE_HTU21D_SENSOR'
