import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    CONF_MAC_ADDRESS,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    CONF_ID,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ICON_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

CODEOWNERS = ["@ahpohl"]

DEPENDENCIES = ["esp32_ble_tracker"]

atc_mithermometer_ns = cg.esphome_ns.namespace("atc_mithermometer")
ATCMiThermometer = atc_mithermometer_ns.class_(
    "ATCMiThermometer", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ATCMiThermometer),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                UNIT_CELSIUS,
                ICON_EMPTY,
                1,
                DEVICE_CLASS_TEMPERATURE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                UNIT_PERCENT,
                ICON_EMPTY,
                0,
                DEVICE_CLASS_HUMIDITY,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                UNIT_PERCENT,
                ICON_EMPTY,
                0,
                DEVICE_CLASS_BATTERY,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
                UNIT_VOLT, ICON_EMPTY, 3, DEVICE_CLASS_VOLTAGE, STATE_CLASS_MEASUREMENT
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
    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))
    if CONF_BATTERY_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_level(sens))
    if CONF_BATTERY_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_VOLTAGE])
        cg.add(var.set_battery_voltage(sens))
