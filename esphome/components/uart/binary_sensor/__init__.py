import esphome.codegen as cg
from esphome.components import binary_sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_DATA
from esphome.core import HexInt

from .. import uart_ns, validate_raw_data

DEPENDENCIES = ["uart"]

UARTBinarySensor = uart_ns.class_("UARTBinarySensor", uart.UARTDevice, cg.Component)


CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(UARTBinarySensor)
    .extend(
        {
            cv.Required(CONF_DATA): validate_raw_data,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = [HexInt(x) for x in data]
    cg.add(var.set_data(cg.ArrayInitializer(*data)))
