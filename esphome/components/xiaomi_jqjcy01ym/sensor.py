import esphome.codegen as cg
from esphome.components import ble_device_base, sensor, xiaomi_ble
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_FORMALDEHYDE,
    CONF_HUMIDITY,
    CONF_MAC_ADDRESS,
    CONF_TEMPERATURE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_FLASK_OUTLINE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MILLIGRAMS_PER_CUBIC_METER,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base", "xiaomi_ble"]

xiaomi_jqjcy01ym_ns = cg.esphome_ns.namespace("xiaomi_jqjcy01ym")
XiaomiJQJCY01YM = xiaomi_jqjcy01ym_ns.class_(
    "XiaomiJQJCY01YM", xiaomi_ble.XiaomiDevice, cg.Component
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_jqjcy01ym"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiJQJCY01YM),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FORMALDEHYDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIGRAMS_PER_CUBIC_METER,
                icon=ICON_FLASK_OUTLINE,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = await ble_device_base.new_ble_device_component(config)

    xiaomi_ble.setup_xiaomi_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)
    await sensors(CONF_FORMALDEHYDE, var.set_formaldehyde_sensor)
    await sensors(CONF_BATTERY_LEVEL, var.set_battery_level_sensor)
