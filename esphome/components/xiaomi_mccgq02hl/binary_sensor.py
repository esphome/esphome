import esphome.codegen as cg
from esphome.components import binary_sensor, ble_device_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BINDKEY,
    CONF_LIGHT,
    CONF_MAC_ADDRESS,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_LIGHT,
    DEVICE_CLASS_OPENING,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base", "sensor"]

xiaomi_mccgq02hl_ns = cg.esphome_ns.namespace("xiaomi_mccgq02hl")
XiaomiMCCGQ02HL = xiaomi_mccgq02hl_ns.class_(
    "XiaomiMCCGQ02HL",
    binary_sensor.BinarySensor,
    cg.Component,
    ble_device_base.ESPBTDeviceListener,
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_mccgq02hl"),
    binary_sensor.binary_sensor_schema(
        XiaomiMCCGQ02HL, device_class=DEVICE_CLASS_OPENING
    )
    .extend(
        {
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Required(CONF_BINDKEY): cv.bind_key,
            cv.Optional(CONF_LIGHT): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_LIGHT
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_bindkey(config[CONF_BINDKEY]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_BATTERY_LEVEL, var.set_battery_level)
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_LIGHT, var.set_light)
