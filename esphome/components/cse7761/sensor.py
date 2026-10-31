import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_VOLTAGE,
    DEVICE_CLASS_POWER_FACTOR,
    STATE_CLASS_MEASUREMENT,
)
from esphome.types import ConfigType

CODEOWNERS = ["@berfenger"]
DEPENDENCIES = ["uart"]

cse7761_ns = cg.esphome_ns.namespace("cse7761")
CSE7761Component = cse7761_ns.class_(
    "CSE7761Component", cg.PollingComponent, uart.UARTDevice
)

CONF_CURRENT_1 = "current_1"
CONF_CURRENT_2 = "current_2"
CONF_ACTIVE_POWER_1 = "active_power_1"
CONF_ACTIVE_POWER_2 = "active_power_2"
CONF_ENERGY_1 = "energy_1"
CONF_ENERGY_2 = "energy_2"
CONF_POWER_FACTOR_1 = "power_factor_1"
CONF_CURRENT_INPUT = "current_input"

CSE7761CurrentInput = cse7761_ns.enum("CSE7761CurrentInput", is_class=True)
CURRENT_INPUTS = CSE7761CurrentInput.options(
    "shunt", "current_transformer", prefix="CSE7761_CURRENT_INPUT_"
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CSE7761Component),
            cv.Optional(CONF_CURRENT_INPUT, default="shunt"): cv.enum(
                CURRENT_INPUTS, lower=True
            ),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                kind=sensor.VOLTAGE, accuracy_decimals=1
            ),
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                kind=sensor.FREQUENCY, accuracy_decimals=2
            ),
            cv.Optional(CONF_POWER_FACTOR_1): sensor.sensor_schema(
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER_FACTOR,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT_1): sensor.sensor_schema(
                kind=sensor.CURRENT, accuracy_decimals=2
            ),
            cv.Optional(CONF_CURRENT_2): sensor.sensor_schema(
                kind=sensor.CURRENT, accuracy_decimals=2
            ),
            cv.Optional(CONF_ACTIVE_POWER_1): sensor.sensor_schema(
                kind=sensor.POWER, accuracy_decimals=1
            ),
            cv.Optional(CONF_ACTIVE_POWER_2): sensor.sensor_schema(
                kind=sensor.POWER, accuracy_decimals=1
            ),
            cv.Optional(CONF_ENERGY_1): sensor.sensor_schema(
                kind=sensor.ENERGY_WH, accuracy_decimals=3
            ),
            cv.Optional(CONF_ENERGY_2): sensor.sensor_schema(
                kind=sensor.ENERGY_WH, accuracy_decimals=3
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "cse7761",
    baud_rate=38400,
    require_rx=True,
    require_tx=True,
    data_bits=8,
    parity="EVEN",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config, config[CONF_CURRENT_INPUT])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_FREQUENCY, var.set_frequency_sensor)
    await sensors(CONF_POWER_FACTOR_1, var.set_power_factor_1_sensor)
    await sensors(CONF_CURRENT_1, var.set_current_1_sensor)
    await sensors(CONF_CURRENT_2, var.set_current_2_sensor)
    await sensors(CONF_ACTIVE_POWER_1, var.set_active_power_1_sensor)
    await sensors(CONF_ACTIVE_POWER_2, var.set_active_power_2_sensor)
    await sensors(CONF_ENERGY_1, var.set_energy_1_sensor)
    await sensors(CONF_ENERGY_2, var.set_energy_2_sensor)
