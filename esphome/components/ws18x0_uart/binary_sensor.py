import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, ws18x0_uart
from esphome.const import CONF_UID
from . import ws18x0_uart_ns

DEPENDENCIES = ["ws18x0_uart"]

CONF_WS18x0_UART_ID = "ws18x0_uart_id"
WS18x0UARTBinarySensor = ws18x0_uart_ns.class_(
    "WS18x0UARTBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(WS18x0UARTBinarySensor).extend(
    {
        cv.GenerateID(CONF_WS18x0_UART_ID): cv.use_id(ws18x0_uart.WS18x0UARTComponent),
        cv.Required(CONF_UID): cv.uint32_t,
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)

    hub = await cg.get_variable(config[CONF_WS18x0_UART_ID])
    cg.add(hub.register_card(var))
    cg.add(var.set_id(config[CONF_UID]))
