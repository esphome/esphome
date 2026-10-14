"""Local controller for Haier hOn water heaters."""

import esphome.codegen as cg
from esphome.components import binary_sensor, sensor, text_sensor, uart, water_heater
import esphome.config_validation as cv
from esphome.types import ConfigType

CONF_COMMUNICATION_TIMEOUT = "communication_timeout"

CODEOWNERS = ["@TheLexus"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor"]

haier_ns = cg.esphome_ns.namespace("haier")
HonWaterHeater = haier_ns.class_(
    "HonWaterHeater", water_heater.WaterHeater, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = (
    water_heater.water_heater_schema(HonWaterHeater)
    .extend(
        {
            cv.Optional(
                CONF_COMMUNICATION_TIMEOUT, default="20s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional("primary_temperature"): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=0,
                device_class="temperature",
                state_class="measurement",
            ),
            cv.Optional("secondary_temperature"): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=0,
                device_class="temperature",
                state_class="measurement",
            ),
            cv.Optional("target_temperature"): sensor.sensor_schema(
                unit_of_measurement="°C",
                accuracy_decimals=0,
                device_class="temperature",
                state_class="measurement",
            ),
            cv.Optional("estimated_heating_time"): sensor.sensor_schema(
                unit_of_measurement="min",
                accuracy_decimals=0,
                device_class="duration",
                state_class="measurement",
            ),
            cv.Optional("available_water"): sensor.sensor_schema(
                unit_of_measurement="%", accuracy_decimals=0, state_class="measurement"
            ),
            cv.Optional("communication"): binary_sensor.binary_sensor_schema(
                device_class="connectivity", entity_category="diagnostic"
            ),
            cv.Optional("power"): binary_sensor.binary_sensor_schema(),
            cv.Optional("heating"): binary_sensor.binary_sensor_schema(
                device_class="heat"
            ),
            cv.Optional("mode"): text_sensor.text_sensor_schema(),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


def FINAL_VALIDATE_SCHEMA(config):
    return uart.final_validate_device_schema(
        "haier.water_heater",
        require_rx=True,
        require_tx=True,
    )(config)


async def to_code(config: ConfigType) -> None:
    var = await water_heater.new_water_heater(config)
    await cg.register_component(var, config)
    cg.add(var.set_communication_timeout(config[CONF_COMMUNICATION_TIMEOUT]))
    await uart.register_uart_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors("primary_temperature", var.set_primary_temperature_sensor)
    await sensors("secondary_temperature", var.set_secondary_temperature_sensor)
    await sensors("target_temperature", var.set_target_temperature_sensor)
    await sensors("estimated_heating_time", var.set_estimated_heating_time_sensor)
    await sensors("available_water", var.set_available_water_sensor)
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors("communication", var.set_communication_sensor)
    await binary_sensors("power", var.set_power_sensor)
    await binary_sensors("heating", var.set_heating_sensor)
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors("mode", var.set_mode_sensor)
