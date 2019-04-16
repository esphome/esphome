import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_NAME, CONF_PIN, CONF_UPDATE_INTERVAL, \
    CONF_ICON, ICON_FLASH, CONF_UNIT_OF_MEASUREMENT, UNIT_AMPERE, CONF_ACCURACY_DECIMALS

CONF_CALIBRATION = 'calibration'
CONF_SAMPLE_SIZE = 'sample_size'
CONF_SUPPLY_VOLTAGE = 'supply_voltage'

ct_clamp_ns = cg.esphome_ns.namespace('ct_clamp')
CTClampSensor = ct_clamp_ns.class_('CTClampSensor', sensor.PollingSensorComponent)

CONFIG_SCHEMA = cv.nameable(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(CTClampSensor),
    cv.Required(CONF_PIN): pins.analog_pin,
    cv.Required(CONF_CALIBRATION): cv.positive_int,

    cv.Optional(CONF_SAMPLE_SIZE, default=1480): cv.positive_int,
    cv.Optional(CONF_SUPPLY_VOLTAGE, default='1V'): cv.voltage,
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
    cv.Optional(CONF_ICON, default=ICON_FLASH): sensor.icon,
    cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_AMPERE): sensor.unit_of_measurement,
    cv.Optional(CONF_ACCURACY_DECIMALS, default=2): sensor.accuracy_decimals,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    pin = config[CONF_PIN]

    rhs = CTClampSensor.new(config[CONF_NAME], pin, config.get(CONF_CALIBRATION),
                            config.get(CONF_SAMPLE_SIZE), config.get(CONF_SUPPLY_VOLTAGE),
                            config.get(CONF_UPDATE_INTERVAL))
    ct_clamp = cg.Pvariable(config[CONF_ID], rhs)
    yield cg.register_component(ct_clamp, config)
    yield sensor.register_sensor(ct_clamp, config)
