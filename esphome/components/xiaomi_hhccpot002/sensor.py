import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import (
    CONF_MAC_ADDRESS,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    ICON_WATER_PERCENT,
    CONF_ID,
    CONF_MOISTURE,
    CONF_CONDUCTIVITY,
    UNIT_MICROSIEMENS_PER_CENTIMETER,
    ICON_FLOWER,
)

DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble"]

xiaomi_hhccpot002_ns = cg.esphome_ns.namespace("xiaomi_hhccpot002")
XiaomiHHCCPOT002 = xiaomi_hhccpot002_ns.class_(
    "XiaomiHHCCPOT002", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiHHCCPOT002),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_MOISTURE): sensor.sensor_schema(
                UNIT_PERCENT,
                ICON_WATER_PERCENT,
                0,
                DEVICE_CLASS_EMPTY,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CONDUCTIVITY): sensor.sensor_schema(
                UNIT_MICROSIEMENS_PER_CENTIMETER,
                ICON_FLOWER,
                0,
                DEVICE_CLASS_EMPTY,
                STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    if CONF_MOISTURE in config:
        sens = await sensor.new_sensor(config[CONF_MOISTURE])
        cg.add(var.set_moisture(sens))
    if CONF_CONDUCTIVITY in config:
        sens = await sensor.new_sensor(config[CONF_CONDUCTIVITY])
        cg.add(var.set_conductivity(sens))
