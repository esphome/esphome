from esphome.components import sensor, uart
from esphome.components.uart import UARTComponent
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_CO2, CONF_ID, CONF_NAME, CONF_TEMPERATURE, CONF_UART_ID, \
    CONF_UPDATE_INTERVAL


DEPENDENCIES = ['uart']

MHZ19Component = sensor.sensor_ns.class_('MHZ19Component', PollingComponent, uart.UARTDevice)
MHZ19TemperatureSensor = sensor.sensor_ns.class_('MHZ19TemperatureSensor',
                                                 sensor.EmptyPollingParentSensor)
MHZ19CO2Sensor = sensor.sensor_ns.class_('MHZ19CO2Sensor', sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MHZ19Component),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
    cv.Required(CONF_CO2): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MHZ19CO2Sensor),
    })),
    cv.Optional(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MHZ19TemperatureSensor),
    })),
    cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    uart_ = yield get_variable(config[CONF_UART_ID])
    rhs = App.make_mhz19_sensor(uart_, config[CONF_CO2][CONF_NAME],
                                config.get(CONF_UPDATE_INTERVAL))
    mhz19 = Pvariable(config[CONF_ID], rhs)
    sensor.setup_sensor(mhz19.Pget_co2_sensor(), config[CONF_CO2])

    if CONF_TEMPERATURE in config:
        sensor.register_sensor(mhz19.Pmake_temperature_sensor(config[CONF_TEMPERATURE][CONF_NAME]),
                               config[CONF_TEMPERATURE])

    register_component(mhz19, config)


BUILD_FLAGS = '-DUSE_MHZ19'
