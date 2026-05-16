import esphome.codegen as cg
from esphome.components import esp32_ble_tracker, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINDKEY,
    CONF_ID,
    CONF_IMPEDANCE,
    CONF_MAC_ADDRESS,
    CONF_WEIGHT,
    DEVICE_CLASS_WEIGHT,
    STATE_CLASS_MEASUREMENT,
    UNIT_KILOGRAM,
)

CONF_IMPEDANCE_LOW = "impedance_low"  # low frequency  50 kHz — larger value
CONF_IMPEDANCE_HIGH = "impedance_high"  # high frequency 250 kHz — smaller value
CONF_HEART_RATE = "heart_rate"
CONF_PROFILE_ID = "profile_id"
UNIT_OHM = "Ω"
UNIT_BPM = "bpm"

DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble"]

xiaomi_body_scale_s400_ns = cg.esphome_ns.namespace("xiaomi_body_scale_s400")
XiaomiBodyScaleS400 = xiaomi_body_scale_s400_ns.class_(
    "XiaomiBodyScaleS400", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

IMPEDANCE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_OHM,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
    icon="mdi:omega",
)

CONFIG_SCHEMA = (
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
            cv.Optional(CONF_IMPEDANCE): IMPEDANCE_SCHEMA,
            cv.Optional(CONF_IMPEDANCE_LOW): IMPEDANCE_SCHEMA,  # 50 kHz  — larger value
            cv.Optional(
                CONF_IMPEDANCE_HIGH
            ): IMPEDANCE_SCHEMA,  # 250 kHz — smaller value
            cv.Optional(CONF_HEART_RATE): sensor.sensor_schema(
                unit_of_measurement=UNIT_BPM,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                icon="mdi:heart-pulse",
            ),
            cv.Optional(CONF_PROFILE_ID): sensor.sensor_schema(
                accuracy_decimals=0,
                icon="mdi:identifier",
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
    cg.add(var.set_bindkey(config[CONF_BINDKEY]))

    if CONF_WEIGHT in config:
        sens = await sensor.new_sensor(config[CONF_WEIGHT])
        cg.add(var.set_weight(sens))
    if CONF_IMPEDANCE in config:
        sens = await sensor.new_sensor(config[CONF_IMPEDANCE])
        cg.add(var.set_impedance(sens))
    if CONF_IMPEDANCE_LOW in config:
        sens = await sensor.new_sensor(config[CONF_IMPEDANCE_LOW])
        cg.add(var.set_impedance_low(sens))
    if CONF_IMPEDANCE_HIGH in config:
        sens = await sensor.new_sensor(config[CONF_IMPEDANCE_HIGH])
        cg.add(var.set_impedance_high(sens))
    if CONF_HEART_RATE in config:
        sens = await sensor.new_sensor(config[CONF_HEART_RATE])
        cg.add(var.set_heart_rate(sens))
    if CONF_PROFILE_ID in config:
        sens = await sensor.new_sensor(config[CONF_PROFILE_ID])
        cg.add(var.set_profile_id(sens))
