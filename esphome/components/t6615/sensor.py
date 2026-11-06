import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_CO2
from esphome.types import ConfigType

CODEOWNERS = ["@tylermenezes"]
DEPENDENCIES = ["uart"]

t6615_ns = cg.esphome_ns.namespace("t6615")
T6615Component = t6615_ns.class_("T6615Component", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(T6615Component),
            cv.Optional(CONF_CO2): sensor.sensor_schema(kind=sensor.CARBON_DIOXIDE),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "t6615",
    baud_rate=19200,
    require_rx=True,
    require_tx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_CO2, var.set_co2_sensor)
