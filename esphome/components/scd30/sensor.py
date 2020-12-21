import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, UNIT_PARTS_PER_MILLION, \
    CONF_HUMIDITY, CONF_TEMPERATURE, ICON_MOLECULE_CO2, \
    UNIT_CELSIUS, ICON_THERMOMETER, ICON_WATER_PERCENT, UNIT_PERCENT, CONF_CO2

DEPENDENCIES = ['i2c']

scd30_ns = cg.esphome_ns.namespace('scd30')
SCD30Component = scd30_ns.class_('SCD30Component', cg.PollingComponent, i2c.I2CDevice)

CONF_AUTOMATIC_SELF_CALIBRATION = 'automatic_self_calibration'
CONF_ALTITUDE_COMPENSATION = 'altitude_compensation'
CONF_AMBIENT_PRESSURE_COMPENSATION = 'ambient_pressure_compensation'
CONF_TEMPERATURE_OFFSET = 'temperature_offset'


def remove_altitude_suffix(value):
    return re.sub(r"\s*(?:m(?:\s+a\.s\.l)?)|(?:MAM?SL)$", '', value)


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SCD30Component),
    cv.Required(CONF_CO2): sensor.sensor_schema(UNIT_PARTS_PER_MILLION,
                                                ICON_MOLECULE_CO2, 0),
    cv.Required(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Required(CONF_HUMIDITY): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 1),
    cv.Optional(CONF_AUTOMATIC_SELF_CALIBRATION, default=True): cv.boolean,
    cv.Optional(CONF_ALTITUDE_COMPENSATION): cv.All(remove_altitude_suffix,
                                                    cv.int_range(min=0, max=0xFFFF,
                                                                 max_included=False)),
    cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION, default=0): cv.pressure,
    cv.Optional(CONF_TEMPERATURE_OFFSET): cv.temperature,
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x61))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    cg.add(var.set_automatic_self_calibration(config[CONF_AUTOMATIC_SELF_CALIBRATION]))
    if CONF_ALTITUDE_COMPENSATION in config:
        cg.add(var.set_altitude_compensation(config[CONF_ALTITUDE_COMPENSATION]))

    if CONF_AMBIENT_PRESSURE_COMPENSATION in config:
        cg.add(var.set_ambient_pressure_compensation(config[CONF_AMBIENT_PRESSURE_COMPENSATION]))

    if CONF_TEMPERATURE_OFFSET in config:
        cg.add(var.set_temperature_offset(config[CONF_TEMPERATURE_OFFSET]))

    if CONF_CO2 in config:
        sens = yield sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
