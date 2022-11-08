import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, binary_sensor, esp32_ble_tracker
from esphome.const import (
    CONF_MAC_ADDRESS,
    CONF_BINDKEY,
    CONF_OPEN,
    CONF_HAS_LIGHT,
    CONF_BATTERY_LEVEL,
    UNIT_PERCENT,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_LIGHT,
    DEVICE_CLASS_OPENING,
    STATE_CLASS_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble", "sensor"]

xiaomi_mccgq02hl_ns = cg.esphome_ns.namespace("xiaomi_mccgq02hl")
XiaomiMCCGQ02HL = xiaomi_mccgq02hl_ns.class_(
    "XiaomiMCCGQ02HL",
    binary_sensor.BinarySensor,
    esp32_ble_tracker.ESPBTDeviceListener,
    cg.Component,
)

CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(XiaomiMCCGQ02HL)
    .extend(
        {
            cv.Required(CONF_BINDKEY): cv.bind_key,
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_HAS_LIGHT): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_LIGHT
            ),
            cv.Optional(CONF_OPEN): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_OPENING
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
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_bindkey(config[CONF_BINDKEY]))

    if CONF_HAS_LIGHT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HAS_LIGHT])
        cg.add(var.set_light(sens))
    if CONF_OPEN in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_OPEN])
        cg.add(var.set_open(sens))
    if CONF_BATTERY_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_level(sens))
