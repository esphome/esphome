import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_CURRENT, CONF_ENERGY, CONF_POWER, CONF_VOLTAGE
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]

pzem004t_ns = cg.esphome_ns.namespace("pzem004t")
PZEM004T = pzem004t_ns.class_("PZEM004T", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PZEM004T),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                kind=sensor.VOLTAGE, accuracy_decimals=1
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                kind=sensor.CURRENT, accuracy_decimals=2
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                kind=sensor.POWER, accuracy_decimals=0
            ),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                kind=sensor.ENERGY_WH, accuracy_decimals=0
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "pzem004t", baud_rate=9600, require_rx=True, require_tx=True
)


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_POWER, var.set_power_sensor)
    await sensors(CONF_ENERGY, var.set_energy_sensor)
