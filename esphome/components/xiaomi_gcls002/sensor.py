import esphome.codegen as cg
from esphome.components import ble_device_base, sensor, xiaomi_ble
import esphome.config_validation as cv
from esphome.const import (
    CONF_CONDUCTIVITY,
    CONF_ILLUMINANCE,
    CONF_MAC_ADDRESS,
    CONF_MOISTURE,
    CONF_TEMPERATURE,
    ICON_FLOWER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROSIEMENS_PER_CENTIMETER,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base", "xiaomi_ble"]

xiaomi_gcls002_ns = cg.esphome_ns.namespace("xiaomi_gcls002")
XiaomiGCLS002 = xiaomi_gcls002_ns.class_(
    "XiaomiGCLS002", xiaomi_ble.XiaomiDevice, cg.Component
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_gcls002"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiGCLS002),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                kind=sensor.TEMPERATURE, accuracy_decimals=1
            ),
            cv.Optional(CONF_MOISTURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                kind=sensor.ILLUMINANCE, accuracy_decimals=0
            ),
            cv.Optional(CONF_CONDUCTIVITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROSIEMENS_PER_CENTIMETER,
                icon=ICON_FLOWER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
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
    await sensors(CONF_MOISTURE, var.set_moisture_sensor)
    await sensors(CONF_ILLUMINANCE, var.set_illuminance_sensor)
    await sensors(CONF_CONDUCTIVITY, var.set_conductivity_sensor)
