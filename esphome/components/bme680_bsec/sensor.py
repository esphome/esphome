import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_TEMPERATURE, CONF_PRESSURE, CONF_HUMIDITY, CONF_GAS_RESISTANCE, \
    UNIT_CELSIUS, UNIT_HECTOPASCAL, UNIT_PERCENT, UNIT_OHM, UNIT_PARTS_PER_MILLION, \
    ICON_THERMOMETER, ICON_GAUGE, ICON_WATER_PERCENT, ICON_GAS_CYLINDER
from esphome.core import coroutine
from . import BME680BSECComponent, CONF_BME680_BSEC_ID

DEPENDENCIES = ['bme680_bsec']

CONF_IAQ = 'iaq'
CONF_CO2_EQUIVALENT = 'co2_equivalent'
CONF_BREATH_VOC_EQUIVALENT = 'breath_voc_equivalent'
UNIT_IAQ = 'IAQ'
ICON_TEST_TUBE = 'mdi:test-tube'

TYPES = {
    CONF_TEMPERATURE: 'set_temperature_sensor',
    CONF_PRESSURE: 'set_pressure_sensor',
    CONF_HUMIDITY: 'set_humidity_sensor',
    CONF_GAS_RESISTANCE: 'set_gas_resistance_sensor',
    CONF_IAQ: 'set_iaq_sensor',
    CONF_CO2_EQUIVALENT: 'set_co2_equivalent_sensor',
    CONF_BREATH_VOC_EQUIVALENT: 'set_breath_voc_equivalent_sensor',
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_BME680_BSEC_ID): cv.use_id(BME680BSECComponent),
    cv.Optional(CONF_TEMPERATURE):
        sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_PRESSURE):
        sensor.sensor_schema(UNIT_HECTOPASCAL, ICON_GAUGE, 1),
    cv.Optional(CONF_HUMIDITY):
        sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 1),
    cv.Optional(CONF_GAS_RESISTANCE):
        sensor.sensor_schema(UNIT_OHM, ICON_GAS_CYLINDER, 0),
    cv.Optional(CONF_IAQ):
        sensor.sensor_schema(UNIT_IAQ, ICON_GAUGE, 0),
    cv.Optional(CONF_CO2_EQUIVALENT):
        sensor.sensor_schema(UNIT_PARTS_PER_MILLION, ICON_TEST_TUBE, 1),
    cv.Optional(CONF_BREATH_VOC_EQUIVALENT):
        sensor.sensor_schema(UNIT_PARTS_PER_MILLION, ICON_TEST_TUBE, 1),
})


@coroutine
def setup_conf(config, key, hub, funcName):
    if key in config:
        conf = config[key]
        var = yield sensor.new_sensor(conf)
        func = getattr(hub, funcName)
        cg.add(func(var))


def to_code(config):
    hub = yield cg.get_variable(config[CONF_BME680_BSEC_ID])
    for key, funcName in TYPES.items():
        yield setup_conf(config, key, hub, funcName)
