import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, binary_sensor, esp32_ble_tracker
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_MAC_ADDRESS,
    CONF_TABLET,
    DEVICE_CLASS_BATTERY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    ICON_BUG,
)


DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble", "sensor"]

xiaomi_wx08zm_ns = cg.esphome_ns.namespace("xiaomi_wx08zm")
XiaomiWX08ZM = xiaomi_wx08zm_ns.class_(
    "XiaomiWX08ZM",
    binary_sensor.BinarySensor,
    esp32_ble_tracker.ESPBTDeviceListener,
    cg.Component,
)

CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(XiaomiWX08ZM)
    .extend(
        {
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TABLET): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_BUG,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
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

    if CONF_TABLET in config:
        sens = await sensor.new_sensor(config[CONF_TABLET])
        cg.add(var.set_tablet(sens))
    if CONF_BATTERY_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_level(sens))
