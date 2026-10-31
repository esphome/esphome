import esphome.codegen as cg
from esphome.components import ble_device_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_VOLTAGE,
    CONF_HUMIDITY,
    CONF_ILLUMINANCE,
    CONF_MAC_ADDRESS,
    CONF_MOISTURE,
    CONF_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

CODEOWNERS = ["@rbaron"]

AUTO_LOAD = ["ble_device_base"]

b_parasite_ns = cg.esphome_ns.namespace("b_parasite")
BParasite = b_parasite_ns.class_(
    "BParasite", ble_device_base.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("b_parasite"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BParasite),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE, accuracy_decimals=1
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                kind=sensor.HUMIDITY, accuracy_decimals=1
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                kind=sensor.VOLTAGE,
                accuracy_decimals=3,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_MOISTURE): sensor.sensor_schema(
                kind=sensor.HUMIDITY, accuracy_decimals=1
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                kind=sensor.ILLUMINANCE, accuracy_decimals=0
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = await ble_device_base.new_ble_device_component(config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
    await sensors(CONF_BATTERY_VOLTAGE, var.set_battery_voltage_sensor)
    await sensors(CONF_MOISTURE, var.set_soil_moisture_sensor)
    await sensors(CONF_ILLUMINANCE, var.set_illuminance_sensor)
