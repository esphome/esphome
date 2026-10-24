"""Local controller for Haier hOn water heaters."""

import esphome.codegen as cg
from esphome.components import binary_sensor, sensor, text_sensor, uart, water_heater
import esphome.config_validation as cv
from esphome.const import (
    CONF_MODE,
    CONF_POWER,
    CONF_TARGET_TEMPERATURE,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MINUTE,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

CONF_AVAILABLE_WATER = "available_water"
CONF_COMMUNICATION = "communication"
CONF_COMMUNICATION_TIMEOUT = "communication_timeout"
CONF_ESTIMATED_HEATING_TIME = "estimated_heating_time"
CONF_HEATING = "heating"
CONF_PRIMARY_TEMPERATURE = "primary_temperature"
CONF_SECONDARY_TEMPERATURE = "secondary_temperature"

CODEOWNERS = ["@TheLexus"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor"]

haier_water_heater_ns = cg.esphome_ns.namespace("haier_water_heater")
HonWaterHeater = haier_water_heater_ns.class_(
    "HonWaterHeater", water_heater.WaterHeater, cg.Component, uart.UARTDevice
)

_TEMPERATURE_SENSOR = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    water_heater.water_heater_schema(HonWaterHeater)
    .extend(
        {
            cv.Optional(
                CONF_COMMUNICATION_TIMEOUT, default="20s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PRIMARY_TEMPERATURE): _TEMPERATURE_SENSOR,
            cv.Optional(CONF_SECONDARY_TEMPERATURE): _TEMPERATURE_SENSOR,
            cv.Optional(CONF_TARGET_TEMPERATURE): _TEMPERATURE_SENSOR,
            cv.Optional(CONF_ESTIMATED_HEATING_TIME): sensor.sensor_schema(
                unit_of_measurement=UNIT_MINUTE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AVAILABLE_WATER): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COMMUNICATION): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_POWER): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_HEATING): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_HEAT
            ),
            cv.Optional(CONF_MODE): text_sensor.text_sensor_schema(),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "haier_water_heater",
    baud_rate=9600,
    require_rx=True,
    require_tx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = await water_heater.new_water_heater(config)
    await cg.register_component(var, config)
    cg.add(var.set_communication_timeout(config[CONF_COMMUNICATION_TIMEOUT]))
    await uart.register_uart_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_PRIMARY_TEMPERATURE, var.set_primary_temperature_sensor)
    await sensors(CONF_SECONDARY_TEMPERATURE, var.set_secondary_temperature_sensor)
    await sensors(CONF_TARGET_TEMPERATURE, var.set_target_temperature_sensor)
    await sensors(CONF_ESTIMATED_HEATING_TIME, var.set_estimated_heating_time_sensor)
    await sensors(CONF_AVAILABLE_WATER, var.set_available_water_sensor)
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_COMMUNICATION, var.set_communication_sensor)
    await binary_sensors(CONF_POWER, var.set_power_sensor)
    await binary_sensors(CONF_HEATING, var.set_heating_sensor)
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_MODE, var.set_mode_sensor)
