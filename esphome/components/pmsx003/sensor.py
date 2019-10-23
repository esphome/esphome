import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import CONF_FORMALDEHYDE, CONF_HUMIDITY, CONF_ID, CONF_PM_10_0, \
    CONF_PM_1_0, CONF_PM_2_5, CONF_TEMPERATURE, CONF_TYPE, ICON_CHEMICAL_WEAPON, \
    UNIT_MICROGRAMS_PER_CUBIC_METER, ICON_THERMOMETER, ICON_WATER_PERCENT, UNIT_CELSIUS, \
    UNIT_PERCENT

DEPENDENCIES = ['uart']

pmsx003_ns = cg.esphome_ns.namespace('pmsx003')
PMSX003Component = pmsx003_ns.class_('PMSX003Component', uart.UARTDevice, cg.Component)
PMSX003Sensor = pmsx003_ns.class_('PMSX003Sensor', sensor.Sensor)

TYPE_PMSX003 = 'PMSX003'
TYPE_PMS5003T = 'PMS5003T'
TYPE_PMS5003ST = 'PMS5003ST'

PMSX003Type = pmsx003_ns.enum('PMSX003Type')
PMSX003_TYPES = {
    TYPE_PMSX003: PMSX003Type.PMSX003_TYPE_X003,
    TYPE_PMS5003T: PMSX003Type.PMSX003_TYPE_5003T,
    TYPE_PMS5003ST: PMSX003Type.PMSX003_TYPE_5003ST,
}

SENSORS_TO_TYPE = {
    CONF_PM_1_0: [TYPE_PMSX003, TYPE_PMS5003ST],
    CONF_PM_2_5: [TYPE_PMSX003, TYPE_PMS5003T, TYPE_PMS5003ST],
    CONF_PM_10_0: [TYPE_PMSX003, TYPE_PMS5003ST],
    CONF_TEMPERATURE: [TYPE_PMS5003T, TYPE_PMS5003ST],
    CONF_HUMIDITY: [TYPE_PMS5003T, TYPE_PMS5003ST],
    CONF_FORMALDEHYDE: [TYPE_PMS5003ST],
}


def validate_pmsx003_sensors(value):
    for key, types in SENSORS_TO_TYPE.items():
        if key in value and value[CONF_TYPE] not in types:
            raise cv.Invalid(u"{} does not have {} sensor!".format(value[CONF_TYPE], key))
    return value


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(PMSX003Component),
    cv.Required(CONF_TYPE): cv.enum(PMSX003_TYPES, upper=True),

    cv.Optional(CONF_PM_1_0):
        sensor.sensor_schema(UNIT_MICROGRAMS_PER_CUBIC_METER, ICON_CHEMICAL_WEAPON, 0),
    cv.Optional(CONF_PM_2_5):
        sensor.sensor_schema(UNIT_MICROGRAMS_PER_CUBIC_METER, ICON_CHEMICAL_WEAPON, 0),
    cv.Optional(CONF_PM_10_0):
        sensor.sensor_schema(UNIT_MICROGRAMS_PER_CUBIC_METER, ICON_CHEMICAL_WEAPON, 0),
    cv.Optional(CONF_TEMPERATURE):
        sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_HUMIDITY):
        sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 1),
    cv.Optional(CONF_FORMALDEHYDE):
        sensor.sensor_schema(UNIT_MICROGRAMS_PER_CUBIC_METER, ICON_CHEMICAL_WEAPON, 0),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    cg.add(var.set_type(config[CONF_TYPE]))

    if CONF_PM_1_0 in config:
        sens = yield sensor.new_sensor(config[CONF_PM_1_0])
        cg.add(var.set_pm_1_0_sensor(sens))

    if CONF_PM_2_5 in config:
        sens = yield sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))

    if CONF_PM_10_0 in config:
        sens = yield sensor.new_sensor(config[CONF_PM_10_0])
        cg.add(var.set_pm_10_0_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))

    if CONF_FORMALDEHYDE in config:
        sens = yield sensor.new_sensor(config[CONF_FORMALDEHYDE])
        cg.add(var.set_formaldehyde_sensor(sens))
