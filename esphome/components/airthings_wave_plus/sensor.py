import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, ble_client

from esphome.const import (
    ESP_PLATFORM_ESP32,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_RADON,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    UNIT_PERCENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    ICON_PERCENT,
    ICON_THERMOMETER,
    ICON_RADIOACTIVE,
    ICON_MOLECULE_CO2,
    CONF_ID,
    CONF_RADON,
    CONF_RADON_LONG_TERM,
    CONF_HUMIDITY,
    CONF_TVOC,
    CONF_CO2,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    UNIT_BECQUEREL_PER_CUBIC_METER,
    UNIT_PARTS_PER_MILLION,
    UNIT_PARTS_PER_BILLION,
    ICON_RADIATOR,
    ICON_GAUGE,
)

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = []

airthings_wave_plus_ns = cg.esphome_ns.namespace("airthings_wave_plus")
AirthingsWavePlus = airthings_wave_plus_ns.class_("AirthingsWavePlus", cg.PollingComponent, ble_client.BLEClientNode)

CONF_BLE_CLIENT_ID = "ble_client_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AirthingsWavePlus),
        cv.Optional(CONF_UPDATE_INTERVAL, default="5min"): cv.update_interval,
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            UNIT_PERCENT, ICON_PERCENT, 0, DEVICE_CLASS_HUMIDITY
        ),
        cv.Optional(CONF_RADON): sensor.sensor_schema(
            UNIT_BECQUEREL_PER_CUBIC_METER, ICON_RADIOACTIVE, 0, DEVICE_CLASS_RADON
        ),
        cv.Optional(CONF_RADON_LONG_TERM): sensor.sensor_schema(
            UNIT_BECQUEREL_PER_CUBIC_METER, ICON_RADIOACTIVE, 0, DEVICE_CLASS_RADON
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            UNIT_CELSIUS, ICON_THERMOMETER, 2, DEVICE_CLASS_TEMPERATURE
        ),
        cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
            UNIT_HECTOPASCAL, ICON_GAUGE, 1, DEVICE_CLASS_PRESSURE
        ),
        cv.Optional(CONF_CO2): sensor.sensor_schema(
            UNIT_PARTS_PER_MILLION, ICON_MOLECULE_CO2, 0, DEVICE_CLASS_EMPTY
        ),
        cv.Optional(CONF_TVOC): sensor.sensor_schema(
            UNIT_PARTS_PER_BILLION, ICON_RADIATOR, 0, DEVICE_CLASS_EMPTY
        ),
    }
).extend(cv.COMPONENT_SCHEMA).extend(ble_client.BLE_CLIENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_BLE_CLIENT_ID])
    cg.add(parent.register_ble_node(var))

    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))
    if CONF_RADON in config:
        sens = await sensor.new_sensor(config[CONF_RADON])
        cg.add(var.set_radon(sens))
    if CONF_RADON_LONG_TERM in config:
        sens = await sensor.new_sensor(config[CONF_RADON_LONG_TERM])
        cg.add(var.set_radon_long_term(sens))
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))
    if CONF_PRESSURE in config:
        sens = await sensor.new_sensor(config[CONF_PRESSURE])
        cg.add(var.set_pressure(sens))
    if CONF_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2(sens))
    if CONF_TVOC in config:
        sens = await sensor.new_sensor(config[CONF_TVOC])
        cg.add(var.set_tvoc(sens))
