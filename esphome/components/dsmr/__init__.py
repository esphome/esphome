import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_SENSOR,
    DEVICE_CLASS_POWER,
    ICON_EMPTY,
    CONF_UART_ID,
    UNIT_WATT_HOURS,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor"]

CONF_DSMR_ID = "dsmr_id"

dsmr_ns = cg.esphome_ns.namespace("dsmr_")
DSMR = dsmr_ns.class_("Dsmr", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DSMR),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


def to_code(config):
    uart_component = yield cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)
    yield cg.register_component(var, config)
