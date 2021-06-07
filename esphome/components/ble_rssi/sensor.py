import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import (
    CONF_SERVICE_UUID,
    CONF_MAC_ADDRESS,
    CONF_ID,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL,
    ICON_EMPTY,
)

DEPENDENCIES = ["esp32_ble_tracker"]

ble_rssi_ns = cg.esphome_ns.namespace("ble_rssi")
BLERSSISensor = ble_rssi_ns.class_(
    "BLERSSISensor", sensor.Sensor, cg.Component, esp32_ble_tracker.ESPBTDeviceListener
)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        UNIT_DECIBEL,
        ICON_EMPTY,
        0,
        DEVICE_CLASS_SIGNAL_STRENGTH,
        STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(BLERSSISensor),
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_SERVICE_UUID): esp32_ble_tracker.bt_uuid,
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_MAC_ADDRESS, CONF_SERVICE_UUID),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)
    await sensor.register_sensor(var, config)

    if CONF_MAC_ADDRESS in config:
        cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    if CONF_SERVICE_UUID in config:
        if len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid16_format):
            cg.add(
                var.set_service_uuid16(
                    esp32_ble_tracker.as_hex(config[CONF_SERVICE_UUID])
                )
            )
        elif len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid32_format):
            cg.add(
                var.set_service_uuid32(
                    esp32_ble_tracker.as_hex(config[CONF_SERVICE_UUID])
                )
            )
        elif len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid128_format):
            uuid128 = esp32_ble_tracker.as_hex_array(config[CONF_SERVICE_UUID])
            cg.add(var.set_service_uuid128(uuid128))
