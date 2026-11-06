import esphome.codegen as cg
from esphome.components import binary_sensor, ble_device_base, sensor, xiaomi_ble
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BINDKEY,
    CONF_IDLE_TIME,
    CONF_ILLUMINANCE,
    CONF_MAC_ADDRESS,
    DEVICE_CLASS_MOTION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_TIMELAPSE,
    STATE_CLASS_MEASUREMENT,
    UNIT_MINUTE,
)
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base", "xiaomi_ble", "sensor"]

xiaomi_cgpr1_ns = cg.esphome_ns.namespace("xiaomi_cgpr1")
XiaomiCGPR1 = xiaomi_cgpr1_ns.class_(
    "XiaomiCGPR1",
    binary_sensor.BinarySensor,
    cg.Component,
    xiaomi_ble.XiaomiEncryptedDevice,
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_cgpr1"),
    binary_sensor.binary_sensor_schema(XiaomiCGPR1, device_class=DEVICE_CLASS_MOTION)
    .extend(
        {
            cv.Required(CONF_BINDKEY): cv.bind_key,
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                kind=sensor.BATTERY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_IDLE_TIME): sensor.sensor_schema(
                unit_of_measurement=UNIT_MINUTE,
                icon=ICON_TIMELAPSE,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                kind=sensor.ILLUMINANCE
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)

    xiaomi_ble.setup_xiaomi_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_IDLE_TIME, var.set_idle_time_sensor)
    await sensors(CONF_BATTERY_LEVEL, var.set_battery_level_sensor)
    await sensors(CONF_ILLUMINANCE, var.set_illuminance_sensor)
