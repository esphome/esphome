import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ['uart']
AUTO_LOAD = ["binary_sensor"]
MULTI_CONF = True

CONF_OUTLAW_990_ID = "outlaw_990_id"

outlaw_990_ns = cg.esphome_ns.namespace('outlaw_990')
Outlaw990 = outlaw_990_ns.class_('Outlaw990', cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Outlaw990)
}).extend(cv.polling_component_schema("1000ms")).extend(uart.UART_DEVICE_SCHEMA)

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)
