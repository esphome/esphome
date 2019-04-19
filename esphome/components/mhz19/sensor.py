import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import CONF_CO2, CONF_ID, CONF_TEMPERATURE, ICON_PERIODIC_TABLE_CO2, \
    UNIT_PARTS_PER_MILLION, UNIT_CELSIUS, ICON_THERMOMETER

DEPENDENCIES = ['uart']

mhz19_ns = cg.esphome_ns.namespace('mhz19')
MHZ19Component = mhz19_ns.class_('MHZ19Component', cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MHZ19Component),
    cv.Required(CONF_CO2): sensor.sensor_schema(UNIT_PARTS_PER_MILLION, ICON_PERIODIC_TABLE_CO2, 0),
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 0),
}).extend(cv.polling_component_schema('60s')).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    if CONF_CO2 in config:
        sens = yield sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
