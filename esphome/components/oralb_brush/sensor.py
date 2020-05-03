import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import CONF_HUMIDITY, CONF_MAC_ADDRESS, CONF_TEMPERATURE, \
    CONF_PRESSURE, CONF_ACCELERATION, CONF_ACCELERATION_X, CONF_ACCELERATION_Y, \
    CONF_ACCELERATION_Z, CONF_BATTERY_VOLTAGE, CONF_TX_POWER, \
    CONF_MEASUREMENT_SEQUENCE_NUMBER, CONF_MOVEMENT_COUNTER, UNIT_CELSIUS, \
    ICON_THERMOMETER, UNIT_PERCENT, UNIT_VOLT, UNIT_HECTOPASCAL, UNIT_G, \
    UNIT_DECIBEL_MILLIWATT, UNIT_EMPTY, ICON_WATER_PERCENT, ICON_BATTERY, \
    ICON_GAUGE, ICON_ACCELERATION, ICON_ACCELERATION_X, ICON_ACCELERATION_Y, \
    ICON_ACCELERATION_Z, ICON_SIGNAL, CONF_ID, ICON_EMPTY, CONF_STATE

DEPENDENCIES = ['esp32_ble_tracker']
AUTO_LOAD = ['oralb_ble']

oralb_brush_ns = cg.esphome_ns.namespace('oralb_brush')
OralbBrush = oralb_brush_ns.class_(
    'OralbBrush', esp32_ble_tracker.ESPBTDeviceListener, cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(OralbBrush),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.Optional(CONF_STATE): sensor.sensor_schema(UNIT_EMPTY, ICON_EMPTY, 0),
}).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    if CONF_STATE in config:
        sens = yield sensor.new_sensor(config[CONF_STATE])
        cg.add(var.set_state(sens))
