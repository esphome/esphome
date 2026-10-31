import esphome.codegen as cg
from esphome.components import ble_device_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_CONDUCTIVITY,
    CONF_ILLUMINANCE,
    CONF_MAC_ADDRESS,
    CONF_MOISTURE,
    CONF_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_FLOWER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROSIEMENS_PER_CENTIMETER,
)
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base"]

xiaomi_hhccjcy10_ns = cg.esphome_ns.namespace("xiaomi_hhccjcy10")
XiaomiHHCCJCY10 = xiaomi_hhccjcy10_ns.class_(
    "XiaomiHHCCJCY10", ble_device_base.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_hhccjcy10"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiHHCCJCY10),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE
            ),
            cv.Optional(CONF_MOISTURE): sensor.sensor_schema(
                kind=sensor.PERCENT, icon=ICON_WATER_PERCENT
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                kind=sensor.ILLUMINANCE
            ),
            cv.Optional(CONF_CONDUCTIVITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROSIEMENS_PER_CENTIMETER,
                icon=ICON_FLOWER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                kind=sensor.BATTERY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
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
    await sensors(CONF_MOISTURE, var.set_moisture_sensor)
    await sensors(CONF_ILLUMINANCE, var.set_illuminance_sensor)
    await sensors(CONF_CONDUCTIVITY, var.set_conductivity_sensor)
    await sensors(CONF_BATTERY_LEVEL, var.set_battery_level_sensor)
