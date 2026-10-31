import esphome.codegen as cg
from esphome.components import binary_sensor, ble_device_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINDKEY,
    CONF_MAC_ADDRESS,
    CONF_WEIGHT,
    DEVICE_CLASS_WEIGHT,
    ICON_HEART_PULSE,
    ICON_OMEGA,
    ICON_SCALE_BATHROOM,
    STATE_CLASS_MEASUREMENT,
    UNIT_BEATS_PER_MINUTE,
    UNIT_KILOGRAM,
)
from esphome.types import ConfigType

CONF_IMPEDANCE_LOW = "impedance_low"
CONF_IMPEDANCE_HIGH = "impedance_high"
CONF_HEART_RATE = "heart_rate"
CONF_PROFILE_ID = "profile_id"
CONF_STABILIZED = "stabilized"

AUTO_LOAD = ["ble_device_base", "binary_sensor"]

xiaomi_body_scale_ns = cg.esphome_ns.namespace("xiaomi_body_scale")
XiaomiBodyScale = xiaomi_body_scale_ns.class_(
    "XiaomiBodyScale", ble_device_base.ESPBTDeviceListener, cg.Component
)

IMPEDANCE_SCHEMA = sensor.sensor_schema(
    kind=sensor.RESISTANCE, icon=ICON_OMEGA, accuracy_decimals=1
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiBodyScale),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Required(CONF_BINDKEY): cv.bind_key,
            cv.Optional(CONF_WEIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOGRAM,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_WEIGHT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_IMPEDANCE_LOW): IMPEDANCE_SCHEMA,
            cv.Optional(CONF_IMPEDANCE_HIGH): IMPEDANCE_SCHEMA,
            cv.Optional(CONF_HEART_RATE): sensor.sensor_schema(
                unit_of_measurement=UNIT_BEATS_PER_MINUTE,
                icon=ICON_HEART_PULSE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PROFILE_ID): sensor.sensor_schema(
                icon="mdi:identifier",
                accuracy_decimals=0,
            ),
            cv.Optional(CONF_STABILIZED): binary_sensor.binary_sensor_schema(
                icon=ICON_SCALE_BATHROOM,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await ble_device_base.new_ble_device_component(
        config, config[CONF_MAC_ADDRESS].as_hex, config[CONF_BINDKEY]
    )

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_WEIGHT, var.set_weight_sensor)
    await sensors(CONF_IMPEDANCE_LOW, var.set_impedance_low_sensor)
    await sensors(CONF_IMPEDANCE_HIGH, var.set_impedance_high_sensor)
    await sensors(CONF_HEART_RATE, var.set_heart_rate_sensor)
    await sensors(CONF_PROFILE_ID, var.set_profile_id_sensor)
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_STABILIZED, var.set_stabilized_binary_sensor)
