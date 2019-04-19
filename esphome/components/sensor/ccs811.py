import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_TVOC, CONF_ECO2, \
    CONF_UPDATE_INTERVAL, CONF_BASELINE, CONF_TOPIC, CONF_STATUS_NAME
from esphome.cpp_generator import HexIntLiteral, Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DEPENDENCIES = ['i2c']

CCS811Component = sensor.sensor_ns.class_('CCS811Component', PollingComponent, i2c.I2CDevice)
CCS811eCO2Sensor = sensor.sensor_ns.class_('CCS811eCO2Sensor',
                                                  sensor.EmptyPollingParentSensor)
CCS811TVOCSensor = sensor.sensor_ns.class_('CCS811TVOCSensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(CCS811Component),
    vol.Required(CONF_ECO2): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(CCS811eCO2Sensor),
    })),
    vol.Required(CONF_TVOC): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(CCS811TVOCSensor),
    })),
    vol.Required(CONF_BASELINE): cv.nameable(vol.Schema({
        vol.Required(CONF_NAME): cv.string,
        vol.Required(CONF_TOPIC): cv.publish_topic
    })),
    vol.Required(CONF_STATUS_NAME): cv.string,
    vol.Optional(CONF_ADDRESS, default=0x5A): cv.i2c_address,
    vol.Optional(CONF_UPDATE_INTERVAL, default="30s"): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_ccs811_sensor(config[CONF_ECO2][CONF_NAME],
                                 config[CONF_TVOC][CONF_NAME],
                                 config[CONF_BASELINE][CONF_NAME],
                                 config[CONF_STATUS_NAME],
                                 config[CONF_BASELINE][CONF_TOPIC],
                                 config[CONF_UPDATE_INTERVAL],
                                 config[CONF_ADDRESS])
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
