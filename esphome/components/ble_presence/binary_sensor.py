import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, esp32_ble_tracker
from esphome.const import CONF_MAC_ADDRESS, CONF_SERVICE_UUID, CONF_ID

DEPENDENCIES = ["esp32_ble_tracker"]

ble_presence_ns = cg.esphome_ns.namespace("ble_presence")
BLEPresenceDevice = ble_presence_ns.class_(
    "BLEPresenceDevice",
    binary_sensor.BinarySensor,
    cg.Component,
    esp32_ble_tracker.ESPBTDeviceListener,
)

CONFIG_SCHEMA = cv.All(
    binary_sensor.BINARY_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(BLEPresenceDevice),
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_SERVICE_UUID): esp32_ble_tracker.bt_uuid,
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_MAC_ADDRESS, CONF_SERVICE_UUID),
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)
    yield binary_sensor.register_binary_sensor(var, config)

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
