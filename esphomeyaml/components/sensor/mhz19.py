import voluptuous as vol

from esphomeyaml.components import sensor, uart
from esphomeyaml.components.uart import UARTComponent
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_CO2, CONF_MAKE_ID, CONF_NAME, CONF_TEMPERATURE, CONF_UART_ID, \
    CONF_UPDATE_INTERVAL, CONF_ID
from esphomeyaml.helpers import App, Application, PollingComponent, get_variable, setup_component, \
    variable, Pvariable

DEPENDENCIES = ['uart']

MakeMHZ19Sensor = Application.struct('MakeMHZ19Sensor')
MHZ19Component = sensor.sensor_ns.class_('MHZ19Component', PollingComponent, uart.UARTDevice)
MHZ19TemperatureSensor = sensor.sensor_ns.class_('MHZ19TemperatureSensor',
                                                 sensor.EmptyPollingParentSensor)
MHZ19CO2Sensor = sensor.sensor_ns.class_('MHZ19CO2Sensor', sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MHZ19Component),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeMHZ19Sensor),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
    vol.Required(CONF_CO2): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MHZ19CO2Sensor),
    })),
    vol.Optional(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MHZ19TemperatureSensor),
    })),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for uart_ in get_variable(config[CONF_UART_ID]):
        yield
    rhs = App.make_mhz19_sensor(uart_, config[CONF_CO2][CONF_NAME],
                                config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    mhz19 = make.Pmhz19
    Pvariable(config[CONF_ID], mhz19)
    sensor.setup_sensor(mhz19.Pget_co2_sensor(), make.Pmqtt, config[CONF_CO2])

    if CONF_TEMPERATURE in config:
        sensor.register_sensor(mhz19.Pmake_temperature_sensor(config[CONF_TEMPERATURE][CONF_NAME]),
                               config[CONF_TEMPERATURE])

    setup_component(mhz19, config)


BUILD_FLAGS = '-DUSE_MHZ19'


def to_hass_config(data, config):
    ret = []
    for key in (CONF_CO2, CONF_TEMPERATURE):
        if key in config:
            ret.append(sensor.core_to_hass_config(data, config[key]))
    return ret
