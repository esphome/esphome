import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_APPARENT_POWER,
    CONF_CURRENT,
    CONF_CURRENT_RESISTOR,
    CONF_ID,
    CONF_POWER,
    CONF_POWER_FACTOR,
    CONF_VOLTAGE,
    CONF_VOLTAGE_DIVIDER,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_WATT,
)
from esphome.types import ConfigType

DEPENDENCIES = ["uart"]

hlw8032_ns = cg.esphome_ns.namespace("hlw8032")
HLW8032Component = hlw8032_ns.class_("HLW8032Component", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HLW8032Component),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_APPARENT_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT_AMPS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_APPARENT_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER_FACTOR): sensor.sensor_schema(
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER_FACTOR,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT_RESISTOR, default=0.001): cv.resistance,
            cv.Optional(CONF_VOLTAGE_DIVIDER, default=1.720): cv.positive_float,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "hlw8032", baud_rate=4800, require_rx=True, data_bits=8, parity="EVEN"
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_POWER, var.set_power_sensor)
    await sensors(CONF_APPARENT_POWER, var.set_apparent_power_sensor)
    await sensors(CONF_POWER_FACTOR, var.set_power_factor_sensor)
    cg.add(var.set_current_resistor(config[CONF_CURRENT_RESISTOR]))
    cg.add(var.set_voltage_divider(config[CONF_VOLTAGE_DIVIDER]))
