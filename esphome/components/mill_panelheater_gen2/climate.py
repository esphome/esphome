import esphome.codegen as cg
from esphome.components import climate, sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_POWER,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    UNIT_WATT,
)

CODEOWNERS = ["@owangen"]

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["climate", "sensor"]

CONF_RATED_POWER = "rated_power"

mill_panelheater_gen2_ns = cg.esphome_ns.namespace("mill_panelheater_gen2")
MillPanelHeaterGen2 = mill_panelheater_gen2_ns.class_(
    "MillPanelHeaterGen2", climate.Climate, cg.Component, uart.UARTDevice
)


def validate_power_sensor(config):
    if CONF_POWER in config and CONF_RATED_POWER not in config:
        raise cv.Invalid("rated_power is required when power is configured")
    if CONF_RATED_POWER in config and CONF_POWER not in config:
        raise cv.Invalid("power is required when rated_power is configured")
    return config


CONFIG_SCHEMA = cv.All(
    climate.climate_schema(MillPanelHeaterGen2)
    .extend(
        {
            cv.Optional(CONF_RATED_POWER): cv.positive_not_null_float,
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_power_sensor,
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "mill_panelheater_gen2",
    baud_rate=9600,
    require_rx=True,
    require_tx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_POWER in config:
        power_sensor = await sensor.new_sensor(config[CONF_POWER])
        cg.add(var.set_power_sensor(power_sensor))
        cg.add(var.set_rated_power(config[CONF_RATED_POWER]))
