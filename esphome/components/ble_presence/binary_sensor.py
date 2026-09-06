import esphome.codegen as cg
from esphome.components import binary_sensor, ble_device_base
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
from esphome.types import ConfigType

CONF_IRK = "irk"

AUTO_LOAD = ["ble_device_base"]

ble_presence_ns = cg.esphome_ns.namespace("ble_presence")
BLEPresenceDevice = ble_presence_ns.class_(
    "BLEPresenceDevice",
    binary_sensor.BinarySensor,
    cg.Component,
    ble_device_base.ESPBTDeviceListener,
)


def _validate(config: ConfigType) -> ConfigType:
    if CONF_IBEACON_MAJOR in config and CONF_IBEACON_UUID not in config:
        raise cv.Invalid("iBeacon major identifier requires iBeacon UUID")
    if CONF_IBEACON_MINOR in config and CONF_IBEACON_UUID not in config:
        raise cv.Invalid("iBeacon minor identifier requires iBeacon UUID")
    return config


CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("ble_presence"),
    binary_sensor.binary_sensor_schema(BLEPresenceDevice)
    .extend(
        {
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_IRK): cv.uuid,
            cv.Optional(CONF_SERVICE_UUID): ble_device_base.bt_uuid,
            cv.Optional(CONF_IBEACON_MAJOR): cv.uint16_t,
            cv.Optional(CONF_IBEACON_MINOR): cv.uint16_t,
            cv.Optional(CONF_IBEACON_UUID): ble_device_base.bt_uuid,
            cv.Optional(CONF_TIMEOUT, default="5min"): cv.positive_time_period,
            cv.Optional(CONF_MIN_RSSI): cv.All(
                cv.decibel, cv.int_range(min=-100, max=-30)
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
    cv.has_exactly_one_key(
        CONF_MAC_ADDRESS, CONF_IRK, CONF_SERVICE_UUID, CONF_IBEACON_UUID
    ),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)

    cg.add(var.set_timeout(config[CONF_TIMEOUT].total_milliseconds))
    if min_rssi := config.get(CONF_MIN_RSSI):
        cg.add(var.set_minimum_rssi(min_rssi))

    if mac_address := config.get(CONF_MAC_ADDRESS):
        cg.add(var.set_address(mac_address.as_hex))

    if irk := config.get(CONF_IRK):
        ble_device_base.request_irk_support()
        irk = ble_device_base.as_hex_array(str(irk))
        cg.add(var.set_irk(irk))

    if service_uuid := config.get(CONF_SERVICE_UUID):
        ble_device_base.add_service_uuid(var, service_uuid)

    if ibeacon_uuid := config.get(CONF_IBEACON_UUID):
        ibeacon_uuid = ble_device_base.as_reversed_hex_array(ibeacon_uuid)
        cg.add(var.set_ibeacon_uuid(ibeacon_uuid))

        if (ibeacon_major := config.get(CONF_IBEACON_MAJOR)) is not None:
            cg.add(var.set_ibeacon_major(ibeacon_major))

        if (ibeacon_minor := config.get(CONF_IBEACON_MINOR)) is not None:
            cg.add(var.set_ibeacon_minor(ibeacon_minor))
