import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, ble_client

from esphome.const import (
    CONF_BATTERY_VOLTAGE,
    CONF_HUMIDITY,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    CONF_TVOC,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_PARTS_PER_BILLION,
    UNIT_PERCENT,
    UNIT_VOLT,
)

CODEOWNERS = ["@ncareau", "@jeromelaban", "@kpfleming"]

DEPENDENCIES = ["ble_client"]

CONF_BATTERY_UPDATE_INTERVAL = "battery_update_interval"

airthings_wave_base_ns = cg.esphome_ns.namespace("airthings_wave_base")
AirthingsWaveBase = airthings_wave_base_ns.class_(
    "AirthingsWaveBase", cg.PollingComponent, ble_client.BLEClientNode
)


BASE_SCHEMA = (
    sensor.SENSOR_SCHEMA.extend(
        {
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TVOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
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


async def wave_base_to_code(var, config):
    await cg.register_component(var, config)

    await ble_client.register_ble_node(var, config)

    if config_humidity := config.get(CONF_HUMIDITY):
        sens = await sensor.new_sensor(config_humidity)
        cg.add(var.set_humidity(sens))
    if config_temperature := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(config_temperature)
        cg.add(var.set_temperature(sens))
    if config_pressure := config.get(CONF_PRESSURE):
        sens = await sensor.new_sensor(config_pressure)
        cg.add(var.set_pressure(sens))
    if config_tvoc := config.get(CONF_TVOC):
        sens = await sensor.new_sensor(config_tvoc)
        cg.add(var.set_tvoc(sens))
    if config_battery_voltage := config.get(CONF_BATTERY_VOLTAGE):
        sens = await sensor.new_sensor(config_battery_voltage)
        cg.add(var.set_battery_voltage(sens))
    if config_battery_update_interval := config.get(CONF_BATTERY_UPDATE_INTERVAL):
        cg.add(var.set_battery_update_interval(config_battery_update_interval))
