import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, binary_sensor, esp32_ble_tracker
from esphome.const import (
    CONF_MAC_ADDRESS,
    CONF_ID,
    CONF_BINDKEY,
    CONF_DEVICE_CLASS,
    CONF_LIGHT,
    CONF_BATTERY_LEVEL,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_ILLUMINANCE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
    UNIT_PERCENT,
    CONF_IDLE_TIME,
    CONF_ILLUMINANCE,
    UNIT_MINUTE,
    UNIT_LUX,
    ICON_TIMELAPSE,
)

DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble"]

xiaomi_mjyd02yla_ns = cg.esphome_ns.namespace("xiaomi_mjyd02yla")
XiaomiMJYD02YLA = xiaomi_mjyd02yla_ns.class_(
    "XiaomiMJYD02YLA",
    binary_sensor.BinarySensor,
    cg.Component,
    esp32_ble_tracker.ESPBTDeviceListener,
)

CONFIG_SCHEMA = cv.All(
    binary_sensor.BINARY_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(XiaomiMJYD02YLA),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Required(CONF_BINDKEY): cv.bind_key,
            cv.Optional(
                CONF_DEVICE_CLASS, default="motion"
            ): binary_sensor.device_class,
            cv.Optional(CONF_IDLE_TIME): sensor.sensor_schema(
                unit_of_measurement=UNIT_MINUTE,
                icon=ICON_TIMELAPSE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_NONE,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_LIGHT): binary_sensor.BINARY_SENSOR_SCHEMA.extend(
                {
                    cv.Optional(
                        CONF_DEVICE_CLASS, default="light"
                    ): binary_sensor.device_class,
                }
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
    await binary_sensor.register_binary_sensor(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_bindkey(config[CONF_BINDKEY]))

    if CONF_IDLE_TIME in config:
        sens = await sensor.new_sensor(config[CONF_IDLE_TIME])
        cg.add(var.set_idle_time(sens))
    if CONF_BATTERY_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_level(sens))
    if CONF_ILLUMINANCE in config:
        sens = await sensor.new_sensor(config[CONF_ILLUMINANCE])
        cg.add(var.set_illuminance(sens))
    if CONF_LIGHT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_LIGHT])
        cg.add(var.set_light(sens))
