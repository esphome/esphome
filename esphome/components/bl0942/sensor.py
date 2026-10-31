import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CURRENT,
    CONF_ENERGY,
    CONF_FREQUENCY,
    CONF_LINE_FREQUENCY,
    CONF_POWER,
    CONF_RESET,
    CONF_VOLTAGE,
)
from esphome.types import ConfigType

CONF_CURRENT_REFERENCE = "current_reference"
CONF_ENERGY_REFERENCE = "energy_reference"
CONF_POWER_REFERENCE = "power_reference"
CONF_VOLTAGE_REFERENCE = "voltage_reference"

DEPENDENCIES = ["uart"]

bl0942_ns = cg.esphome_ns.namespace("bl0942")
BL0942 = bl0942_ns.class_("BL0942", cg.PollingComponent, uart.UARTDevice)

LineFrequency = bl0942_ns.enum("LineFrequency")
LINE_FREQS = {
    50: LineFrequency.LINE_FREQUENCY_50HZ,
    60: LineFrequency.LINE_FREQUENCY_60HZ,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BL0942),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                kind=sensor.VOLTAGE, accuracy_decimals=1
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                kind=sensor.CURRENT, accuracy_decimals=2
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                kind=sensor.POWER, accuracy_decimals=1
            ),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                kind=sensor.ENERGY_KWH, accuracy_decimals=3
            ),
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                kind=sensor.FREQUENCY, accuracy_decimals=2
            ),
            cv.Optional(CONF_LINE_FREQUENCY, default="50HZ"): cv.All(
                cv.frequency,
                cv.enum(
                    LINE_FREQS,
                    int=True,
                ),
            ),
            cv.Optional(CONF_ADDRESS, default=0): cv.int_range(min=0, max=3),
            cv.Optional(CONF_RESET, default=True): cv.boolean,
            cv.Optional(CONF_CURRENT_REFERENCE): cv.float_,
            cv.Optional(CONF_ENERGY_REFERENCE): cv.float_,
            cv.Optional(CONF_POWER_REFERENCE): cv.float_,
            cv.Optional(CONF_VOLTAGE_REFERENCE): cv.float_,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_POWER, var.set_power_sensor)
    await sensors(CONF_ENERGY, var.set_energy_sensor)
    await sensors(CONF_FREQUENCY, var.set_frequency_sensor)
    cg.add(var.set_line_freq(config[CONF_LINE_FREQUENCY]))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_reset(config[CONF_RESET]))
    if (current_reference := config.get(CONF_CURRENT_REFERENCE, None)) is not None:
        cg.add(var.set_current_reference(current_reference))
    if (voltage_reference := config.get(CONF_VOLTAGE_REFERENCE, None)) is not None:
        cg.add(var.set_voltage_reference(voltage_reference))
    if (power_reference := config.get(CONF_POWER_REFERENCE, None)) is not None:
        cg.add(var.set_power_reference(power_reference))
    if (energy_reference := config.get(CONF_ENERGY_REFERENCE, None)) is not None:
        cg.add(var.set_energy_reference(energy_reference))
