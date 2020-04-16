import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import time
from esphome.components import sensor, uart
from esphome.const import CONF_CURRENT, CONF_ID, CONF_POWER, CONF_VOLTAGE, \
    CONF_ENERGY, UNIT_VOLT, ICON_FLASH, ICON_COUNTER, UNIT_AMPERE, UNIT_WATT, \
    UNIT_KWATT_HOURS, UNIT_WATT_HOURS, CONF_ENERGY_PER_HOUR, CONF_ENERGY_PER_DAY, \
    CONF_ENERGY_PER_MONTH, CONF_TIME_ID

DEPENDENCIES = ['uart']

pzem004t_ns = cg.esphome_ns.namespace('pzem004t')
PZEM004T = pzem004t_ns.class_('PZEM004T', cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(PZEM004T),

    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_CURRENT): sensor.sensor_schema(UNIT_AMPERE, ICON_FLASH, 2),
    cv.Optional(CONF_POWER): sensor.sensor_schema(UNIT_WATT, ICON_FLASH, 0),
    cv.Optional(CONF_ENERGY): sensor.sensor_schema(UNIT_WATT_HOURS, ICON_COUNTER, 0),
    cv.Optional(CONF_ENERGY_PER_HOUR): sensor.sensor_schema(UNIT_WATT_HOURS, ICON_COUNTER, 0),
    cv.Optional(CONF_ENERGY_PER_DAY): sensor.sensor_schema(UNIT_KWATT_HOURS, ICON_COUNTER, 3),
    cv.Optional(CONF_ENERGY_PER_MONTH): sensor.sensor_schema(UNIT_KWATT_HOURS, ICON_COUNTER, 3),

    cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
}).extend(cv.polling_component_schema('60s')).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)
    time_ = yield cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))
    if CONF_VOLTAGE in config:
        conf = config[CONF_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_voltage_sensor(sens))
    if CONF_CURRENT in config:
        conf = config[CONF_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_current_sensor(sens))
    if CONF_POWER in config:
        conf = config[CONF_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_power_sensor(sens))
    if CONF_ENERGY in config:
        conf = config[CONF_ENERGY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_energy_sensor(sens))
    if CONF_ENERGY_PER_HOUR in config:
        conf = config[CONF_ENERGY_PER_HOUR]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_energy_hour_sensor(sens))
    if CONF_ENERGY_PER_DAY in config:
        conf = config[CONF_ENERGY_PER_DAY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_energy_day_sensor(sens))
    if CONF_ENERGY_PER_MONTH in config:
        conf = config[CONF_ENERGY_PER_MONTH]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.set_energy_month_sensor(sens))
