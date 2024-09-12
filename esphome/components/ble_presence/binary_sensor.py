import esphome.codegen as cg
from esphome.components import binary_sensor, esp32_ble_tracker
import esphome.config_validation as cv
from esphome.const import (
    CONF_IBEACON_MAJOR,
    CONF_IBEACON_MINOR,
    CONF_IBEACON_UUID,
    CONF_MAC_ADDRESS,
    CONF_MIN_RSSI,
    CONF_SERVICE_UUID,
    CONF_TIMEOUT,
)

CONF_IRK = "irk"

DEPENDENCIES = ["esp32_ble_tracker"]

ble_presence_ns = cg.esphome_ns.namespace("ble_presence")
BLEPresenceDevice = ble_presence_ns.class_(
    "BLEPresenceDevice",
    binary_sensor.BinarySensor,
    cg.Component,
    esp32_ble_tracker.ESPBTDeviceListener,
)


def _validate(config):
    if CONF_IBEACON_MAJOR in config and CONF_IBEACON_UUID not in config:
        raise cv.Invalid("iBeacon major identifier requires iBeacon UUID")
    if CONF_IBEACON_MINOR in config and CONF_IBEACON_UUID not in config:
        raise cv.Invalid("iBeacon minor identifier requires iBeacon UUID")
    return config


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(BLEPresenceDevice)
    .extend(
        {
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_IRK): cv.uuid,
            cv.Optional(CONF_SERVICE_UUID): esp32_ble_tracker.bt_uuid,
            cv.Optional(CONF_IBEACON_MAJOR): cv.uint16_t,
            cv.Optional(CONF_IBEACON_MINOR): cv.uint16_t,
            cv.Optional(CONF_IBEACON_UUID): cv.uuid,
            cv.Optional(CONF_TIMEOUT, default="5min"): cv.positive_time_period,
            cv.Optional(CONF_MIN_RSSI): cv.All(
                cv.decibel, cv.int_range(min=-100, max=-30)
            ),
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(
        CONF_MAC_ADDRESS, CONF_IRK, CONF_SERVICE_UUID, CONF_IBEACON_UUID
    ),
    _validate,
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_timeout(config[CONF_TIMEOUT].total_milliseconds))
    if min_rssi := config.get(CONF_MIN_RSSI):
        cg.add(var.set_minimum_rssi(min_rssi))

    if mac_address := config.get(CONF_MAC_ADDRESS):
        cg.add(var.set_address(mac_address.as_hex))

    if irk := config.get(CONF_IRK):
        irk = esp32_ble_tracker.as_hex_array(str(irk))
        cg.add(var.set_irk(irk))

    if service_uuid := config.get(CONF_SERVICE_UUID):
        if len(service_uuid) == len(esp32_ble_tracker.bt_uuid16_format):
            cg.add(var.set_service_uuid16(esp32_ble_tracker.as_hex(service_uuid)))
        elif len(service_uuid) == len(esp32_ble_tracker.bt_uuid32_format):
            cg.add(var.set_service_uuid32(esp32_ble_tracker.as_hex(service_uuid)))
        elif len(service_uuid) == len(esp32_ble_tracker.bt_uuid128_format):
            uuid128 = esp32_ble_tracker.as_reversed_hex_array(service_uuid)
            cg.add(var.set_service_uuid128(uuid128))

    if ibeacon_uuid := config.get(CONF_IBEACON_UUID):
        ibeacon_uuid = esp32_ble_tracker.as_hex_array(str(ibeacon_uuid))
        cg.add(var.set_ibeacon_uuid(ibeacon_uuid))

        if (ibeacon_major := config.get(CONF_IBEACON_MAJOR)) is not None:
            cg.add(var.set_ibeacon_major(ibeacon_major))

        if (ibeacon_minor := config.get(CONF_IBEACON_MINOR)) is not None:
            cg.add(var.set_ibeacon_minor(ibeacon_minor))
