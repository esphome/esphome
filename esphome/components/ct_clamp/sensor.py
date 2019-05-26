import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_PIN, ICON_FLASH, UNIT_AMPERE

CONF_CALIBRATION = 'calibration'
CONF_SAMPLE_SIZE = 'sample_size'
CONF_SUPPLY_VOLTAGE = 'supply_voltage'

ct_clamp_ns = cg.esphome_ns.namespace('ct_clamp')
CTClampSensor = ct_clamp_ns.class_('CTClampSensor', sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_AMPERE, ICON_FLASH, 2).extend({
    cv.GenerateID(): cv.declare_id(CTClampSensor),
    cv.Required(CONF_PIN): pins.analog_pin,
    cv.Required(CONF_CALIBRATION): cv.positive_int,
    cv.Optional(CONF_SAMPLE_SIZE, default=1480): cv.positive_int,
    cv.Optional(CONF_SUPPLY_VOLTAGE, default='1V'): cv.voltage,
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    cg.add(var.set_pin(config[CONF_PIN]))
    cg.add(var.set_calibration(config[CONF_CALIBRATION]))
    cg.add(var.set_sample_size(config[CONF_SAMPLE_SIZE]))
    cg.add(var.set_supply_voltage(config[CONF_SUPPLY_VOLTAGE]))
