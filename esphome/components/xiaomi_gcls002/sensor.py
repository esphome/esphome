import esphome.codegen as cg
from esphome.components import esp32_ble_tracker, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CONDUCTIVITY,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_MAC_ADDRESS,
    CONF_MOISTURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_FLOWER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_LUX,
    UNIT_MICROSIEMENS_PER_CENTIMETER,
    UNIT_PERCENT,
)

DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble"]

xiaomi_gcls002_ns = cg.esphome_ns.namespace("xiaomi_gcls002")
XiaomiGCLS002 = xiaomi_gcls002_ns.class_(
    "XiaomiGCLS002", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiGCLS002),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MOISTURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CONDUCTIVITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROSIEMENS_PER_CENTIMETER,
                icon=ICON_FLOWER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
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

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))
    if CONF_MOISTURE in config:
        sens = await sensor.new_sensor(config[CONF_MOISTURE])
        cg.add(var.set_moisture(sens))
    if CONF_ILLUMINANCE in config:
        sens = await sensor.new_sensor(config[CONF_ILLUMINANCE])
        cg.add(var.set_illuminance(sens))
    if CONF_CONDUCTIVITY in config:
        sens = await sensor.new_sensor(config[CONF_CONDUCTIVITY])
        cg.add(var.set_conductivity(sens))
