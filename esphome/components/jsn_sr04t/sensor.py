import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
    ICON_ARROW_EXPAND_VERTICAL,
    CONF_MODEL,
)

CODEOWNERS = ["@Mafus1"]
DEPENDENCIES = ["uart"]

jsn_sr04t_ns = cg.esphome_ns.namespace("jsn_sr04t")
Jsnsr04tComponent = jsn_sr04t_ns.class_(
    "Jsnsr04tComponent", sensor.Sensor, cg.PollingComponent, uart.UARTDevice
)
Model = jsn_sr04t_ns.enum("Model")
MODEL = {
    "jsn_sr04t": Model.JSN_SR04T,
    "aj_sr04m": Model.AJ_SR04M,
    "rcwl_1655": Model.RCWL_1655,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        Jsnsr04tComponent,
        unit_of_measurement=UNIT_METER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_MODEL, default="jsn_sr04t"): cv.enum(MODEL, upper=False),
        }
    )
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "jsn_sr04t",
    baud_rate=9600,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity=None,
    stop_bits=1,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))
