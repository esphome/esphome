import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import CONF_BATTERY_LEVEL, CONF_HUMIDITY, CONF_MAC_ADDRESS, CONF_TEMPERATURE, \
    UNIT_CELSIUS, ICON_THERMOMETER, UNIT_PERCENT, ICON_WATER_PERCENT, ICON_BATTERY, CONF_ID, \
    CONF_BINDKEY

CODEOWNERS = ['@rbaron']

DEPENDENCIES = ['esp32_ble_tracker']
# AUTO_LOAD = ['xiaomi_ble']

xiaomi_lywsd03mmc_ns = cg.esphome_ns.namespace('parasite')
XiaomiLYWSD03MMC = xiaomi_lywsd03mmc_ns.class_('Parasite',
                                               esp32_ble_tracker.ESPBTDeviceListener,
                                               cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(XiaomiLYWSD03MMC),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 0),
}).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))