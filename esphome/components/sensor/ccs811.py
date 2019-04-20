import voluptuous as vol

from esphome.components import i2c, sensor, time
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_TVOC, CONF_ECO2, \
    CONF_UPDATE_INTERVAL, CONF_BASELINE_NAME, CONF_STATUS_NAME, CONF_SAVE_TOPIC, CONF_TIME_ID
from esphome.cpp_generator import HexIntLiteral, Pvariable, get_variable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DEPENDENCIES = ['i2c', 'time']

CCS811Component = sensor.sensor_ns.class_('CCS811Component', PollingComponent, i2c.I2CDevice)
CCS811eCO2Sensor = sensor.sensor_ns.class_('CCS811eCO2Sensor',
                                                  sensor.EmptyPollingParentSensor)
CCS811TVOCSensor = sensor.sensor_ns.class_('CCS811TVOCSensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(CCS811Component),
    cv.GenerateID(CONF_TIME_ID): cv.use_variable_id(time.RealTimeClockComponent),
    vol.Required(CONF_ECO2): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(CCS811eCO2Sensor),
    })),
    vol.Required(CONF_TVOC): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(CCS811TVOCSensor),
    })),
    vol.Required(CONF_BASELINE_NAME): cv.string,
    vol.Required(CONF_STATUS_NAME): cv.string,
    vol.Required(CONF_SAVE_TOPIC): cv.publish_topic,
    vol.Optional(CONF_ADDRESS, default=0x5A): cv.i2c_address,
    vol.Optional(CONF_UPDATE_INTERVAL, default="30s"): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for time_ in get_variable(config[CONF_TIME_ID]):
        yield
    
    rhs = App.make_ccs811_sensor(config[CONF_ECO2][CONF_NAME],
                                 config[CONF_TVOC][CONF_NAME],
                                 config[CONF_BASELINE_NAME],
                                 config[CONF_STATUS_NAME],
                                 config[CONF_SAVE_TOPIC],
                                 config[CONF_UPDATE_INTERVAL],
                                 config[CONF_ADDRESS],
                                 time_)
    ccs = Pvariable(config[CONF_ID], rhs)
    if CONF_ADDRESS in config:
        add(ccs.set_address(HexIntLiteral(config[CONF_ADDRESS])))

    sensor.setup_sensor(ccs.Pget_eco2_sensor(), config[CONF_ECO2])
    sensor.setup_sensor(ccs.Pget_tvoc_sensor(), config[CONF_TVOC])
    setup_component(ccs, config)


BUILD_FLAGS = '-DUSE_CCS811_SENSOR -DUSE_TEXT_SENSOR -DUSE_SWITCH'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_ECO2]),
            sensor.core_to_hass_config(data, config[CONF_TVOC])]
