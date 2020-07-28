import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import CONF_ID, CONF_HUMIDITY, CONF_TEMPERATURE, CONF_LIGHT, \
    CONF_NOISE_LEVEL, CONF_PM_10_0, CONF_UPDATE_INTERVAL, UNIT_PERCENT, UNIT_CELSIUS, \
    ICON_WATER_PERCENT, ICON_THERMOMETER, ICON_LIGHTBULB, ICON_PULSE, ICON_CHEMICAL_WEAPON

DEPENDENCIES = ['uart']

CONF_HUMIDITY_THRESHOLD = 'humidity_threshold'
CONF_TEMPERATURE_THRESHOLD = 'temperature_threshold'

sonoff_sc_ns = cg.esphome_ns.namespace('sonoff_sc')
SonoffSCComponent = sonoff_sc_ns.class_('SonoffSCComponent', uart.UARTDevice, cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SonoffSCComponent),

    cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 0),
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_LIGHT): sensor.sensor_schema(UNIT_PERCENT, ICON_LIGHTBULB, 0),
    cv.Optional(CONF_NOISE_LEVEL): sensor.sensor_schema(UNIT_PERCENT, ICON_PULSE, 0),
    cv.Optional(CONF_PM_10_0): sensor.sensor_schema(UNIT_PERCENT, ICON_CHEMICAL_WEAPON, 0),
    cv.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_seconds,
    cv.Optional(CONF_HUMIDITY_THRESHOLD, default=1): cv.positive_not_null_int,
    cv.Optional(CONF_TEMPERATURE_THRESHOLD, default=1): cv.positive_not_null_int
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    if CONF_UPDATE_INTERVAL in config:
        cg.add(var.set_update_interval_sec(config[CONF_UPDATE_INTERVAL]))

    if CONF_HUMIDITY_THRESHOLD in config:
        cg.add(var.set_humidity_threshold(config[CONF_HUMIDITY_THRESHOLD]))

    if CONF_TEMPERATURE_THRESHOLD in config:
        cg.add(var.set_temperature_threshold(config[CONF_TEMPERATURE_THRESHOLD]))

    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_temperature_sensor(sens))

    if CONF_HUMIDITY in config:
        conf = config[CONF_HUMIDITY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_humidity_sensor(sens))

    if CONF_LIGHT in config:
        conf = config[CONF_LIGHT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_light_sensor(sens))

    if CONF_NOISE_LEVEL in config:
        conf = config[CONF_NOISE_LEVEL]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_noise_sensor(sens))

    if CONF_PM_10_0 in config:
        conf = config[CONF_PM_10_0]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_dust_sensor(sens))
