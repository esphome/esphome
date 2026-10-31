import esphome.codegen as cg
from esphome.components import ble_client, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_VOLTAGE,
    CONF_HUMIDITY,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    CONF_TVOC,
    DEVICE_CLASS_HUMIDITY as DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PRESSURE as DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE as DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    DEVICE_CLASS_VOLTAGE as DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS as UNIT_CELSIUS,
    UNIT_HECTOPASCAL as UNIT_HECTOPASCAL,
    UNIT_PARTS_PER_BILLION,
    UNIT_PERCENT as UNIT_PERCENT,
    UNIT_VOLT as UNIT_VOLT,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@ncareau", "@jeromelaban"]
DOMAIN = "airthings_wave_base"

DEPENDENCIES = ["ble_client"]

CONF_BATTERY_UPDATE_INTERVAL = "battery_update_interval"

airthings_wave_base_ns = cg.esphome_ns.namespace("airthings_wave_base")
AirthingsWaveBase = airthings_wave_base_ns.class_(
    "AirthingsWaveBase", cg.PollingComponent, ble_client.BLEClientNode
)


BASE_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                kind=sensor.HUMIDITY, accuracy_decimals=0
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE, accuracy_decimals=2
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                kind=sensor.PRESSURE, accuracy_decimals=1
            ),
            cv.Optional(CONF_TVOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                kind=sensor.VOLTAGE,
                accuracy_decimals=3,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(
                CONF_BATTERY_UPDATE_INTERVAL,
                default="24h",
            ): cv.update_interval,
        }
    )
    .extend(cv.polling_component_schema("5min"))
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def wave_base_to_code(var: MockObj, config: ConfigType) -> None:
    await cg.register_component(var, config)

    await ble_client.register_ble_node(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_PRESSURE, var.set_pressure_sensor)
    await sensors(CONF_TVOC, var.set_tvoc_sensor)
    await sensors(CONF_BATTERY_VOLTAGE, var.set_battery_voltage)
    if config_battery_update_interval := config.get(CONF_BATTERY_UPDATE_INTERVAL):
        cg.add(var.set_battery_update_interval(config_battery_update_interval))
