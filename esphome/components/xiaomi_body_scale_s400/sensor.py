import esphome.codegen as cg
from esphome.components import binary_sensor, ble_device_base, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINDKEY,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_WEIGHT,
    DEVICE_CLASS_WEIGHT,
    ICON_HEART_PULSE,
    ICON_OMEGA,
    ICON_SCALE_BATHROOM,
    STATE_CLASS_MEASUREMENT,
    UNIT_BEATS_PER_MINUTE,
    UNIT_KILOGRAM,
    UNIT_OHM,
)
from esphome.types import ConfigType

CONF_IMPEDANCE_LOW = "impedance_low"  # 50 kHz, the larger value
CONF_IMPEDANCE_HIGH = "impedance_high"  # 250 kHz, the smaller value
CONF_HEART_RATE = "heart_rate"
CONF_PROFILE_ID = "profile_id"
CONF_STABILIZED = "stabilized"

AUTO_LOAD = ["ble_device_base", "binary_sensor"]

xiaomi_body_scale_s400_ns = cg.esphome_ns.namespace("xiaomi_body_scale_s400")
XiaomiBodyScaleS400 = xiaomi_body_scale_s400_ns.class_(
    "XiaomiBodyScaleS400", ble_device_base.ESPBTDeviceListener, cg.Component
)

IMPEDANCE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_OHM,
    icon=ICON_OMEGA,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_body_scale_s400"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiBodyScaleS400),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Required(CONF_BINDKEY): cv.bind_key,
            cv.Optional(CONF_WEIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOGRAM,
                accuracy_decimals=1,
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
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_bindkey(config[CONF_BINDKEY]))

    for key, setter in (
        (CONF_WEIGHT, var.set_weight),
        (CONF_IMPEDANCE_LOW, var.set_impedance_low),
        (CONF_IMPEDANCE_HIGH, var.set_impedance_high),
        (CONF_HEART_RATE, var.set_heart_rate),
        (CONF_PROFILE_ID, var.set_profile_id),
    ):
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(setter(sens))
    if (conf := config.get(CONF_STABILIZED)) is not None:
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_stabilized(sens))
