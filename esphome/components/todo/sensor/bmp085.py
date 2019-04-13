from esphome.components import i2c, sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_PRESSURE, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL


DEPENDENCIES = ['i2c']

BMP085Component = sensor.sensor_ns.class_('BMP085Component', PollingComponent, i2c.I2CDevice)
BMP085TemperatureSensor = sensor.sensor_ns.class_('BMP085TemperatureSensor',
                                                  sensor.EmptyPollingParentSensor)
BMP085PressureSensor = sensor.sensor_ns.class_('BMP085PressureSensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(BMP085Component),
    cv.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BMP085TemperatureSensor),
    })),
    cv.Required(CONF_PRESSURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BMP085PressureSensor),
    })),
    cv.Optional(CONF_ADDRESS): cv.i2c_address,
    cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = App.make_bmp085_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_PRESSURE][CONF_NAME],
                                 config.get(CONF_UPDATE_INTERVAL))
    bmp = Pvariable(config[CONF_ID], rhs)
    if CONF_ADDRESS in config:
        cg.add(bmp.set_address(HexIntLiteral(config[CONF_ADDRESS])))

    sensor.setup_sensor(bmp.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_sensor(bmp.Pget_pressure_sensor(), config[CONF_PRESSURE])
    register_component(bmp, config)


BUILD_FLAGS = '-DUSE_BMP085_SENSOR'
