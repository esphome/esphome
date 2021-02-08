import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import CONF_ID, CONF_CO2, CONF_FORMALDEHYDE, CONF_TVOC, CONF_PM_2_5, \
    CONF_PM_10_0, CONF_TEMPERATURE, CONF_HUMIDITY, UNIT_PARTS_PER_MILLION, \
    UNIT_MICROGRAMS_PER_CUBIC_METER, UNIT_CELSIUS, UNIT_PERCENT, \
    ICON_MOLECULE_CO2, ICON_FLASK, ICON_CHEMICAL_WEAPON, \
    ICON_GRAIN, ICON_THERMOMETER, ICON_WATER_PERCENT

DEPENDENCIES = ['uart']

sm300d2_ns = cg.esphome_ns.namespace('sm300d2')
SM300D2Sensor = sm300d2_ns.class_('SM300D2Sensor', cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(SM300D2Sensor),

    cv.Optional(CONF_CO2):
        sensor.sensor_schema(UNIT_PARTS_PER_MILLION, ICON_MOLECULE_CO2, 0),
    cv.Optional(CONF_FORMALDEHYDE):
        sensor.sensor_schema(UNIT_MICROGRAMS_PER_CUBIC_METER, ICON_FLASK, 0),
    cv.Optional(CONF_TVOC):
        sensor.sensor_schema(UNIT_MICROGRAMS_PER_CUBIC_METER, ICON_CHEMICAL_WEAPON, 0),
    cv.Optional(CONF_PM_2_5):
        sensor.sensor_schema(UNIT_MICROGRAMS_PER_CUBIC_METER, ICON_GRAIN, 0),
    cv.Optional(CONF_PM_10_0):
        sensor.sensor_schema(UNIT_MICROGRAMS_PER_CUBIC_METER, ICON_GRAIN, 0),
    cv.Optional(CONF_TEMPERATURE):
        sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 0),
    cv.Optional(CONF_HUMIDITY):
        sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 0),

}).extend(cv.polling_component_schema('60s')).extend(uart.UART_DEVICE_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    if CONF_CO2 in config:
        sens = yield sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))
    if CONF_FORMALDEHYDE in config:
        sens = yield sensor.new_sensor(config[CONF_FORMALDEHYDE])
        cg.add(var.set_formaldehyde_sensor(sens))
    if CONF_TVOC in config:
        sens = yield sensor.new_sensor(config[CONF_TVOC])
        cg.add(var.set_tvoc_sensor(sens))
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
