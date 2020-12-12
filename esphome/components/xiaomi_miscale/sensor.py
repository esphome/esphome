import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import CONF_MAC_ADDRESS, CONF_ID, CONF_WEIGHT, UNIT_KILOGRAM, \
    ICON_SCALEMI

DEPENDENCIES = ['esp32_ble_tracker']

xiaomi_miscale_ns = cg.esphome_ns.namespace('xiaomi_miscale')
XiaomiMiscale = xiaomi_miscale_ns.class_('XiaomiMiscale',
                                         esp32_ble_tracker.ESPBTDeviceListener,
                                         cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(XiaomiMiscale),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.Optional(CONF_WEIGHT): sensor.sensor_schema(UNIT_KILOGRAM, ICON_SCALEMI, 2),
}).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    if CONF_WEIGHT in config:
        sens = yield sensor.new_sensor(config[CONF_WEIGHT])
        cg.add(var.set_weight(sens))
