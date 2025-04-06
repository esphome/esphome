import esphome.codegen as cg
from esphome.components import esp32_ble_tracker, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINDKEY,
    CONF_CLEAR_IMPEDANCE,
    CONF_ID,
    CONF_IMPEDANCE,
    CONF_MAC_ADDRESS,
    CONF_WEIGHT,
    DEVICE_CLASS_TIMESTAMP,
    DEVICE_CLASS_WEIGHT,
    ICON_HEART_PULSE,
    ICON_OMEGA,
    ICON_SCALE_BATHROOM,
    STATE_CLASS_MEASUREMENT,
    UNIT_BEATS_PER_MINUTE,
    UNIT_KILOGRAM,
    UNIT_OHM,
)

DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble"]

CONF_HEART_RATE = "heart_rate"
CONF_IMPEDANCE_HIGH = "impedance_high"
CONF_IMPEDANCE_LOW = "impedance_low"
CONF_PROFILE_ID = "profile_id"
CONF_TIMESTAMP = "timestamp"
CONF_ALLOWED_PROFILE_IDS = "allowed_profile_ids"

xiaomi_miscale_ns = cg.esphome_ns.namespace("xiaomi_miscale")
XiaomiMiscale = xiaomi_miscale_ns.class_(
    "XiaomiMiscale", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)


def _validate_profile_ids(value):
    if isinstance(value, str) and value.lower() == "any":
        return value
    return cv.uint8_t(value)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiMiscale),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_BINDKEY): cv.bind_key,
            cv.Optional(CONF_CLEAR_IMPEDANCE, default=False): cv.boolean,
            cv.Optional(CONF_WEIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOGRAM,
                icon=ICON_SCALE_BATHROOM,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_WEIGHT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_IMPEDANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_OHM,
                icon=ICON_OMEGA,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_IMPEDANCE_HIGH): sensor.sensor_schema(
                unit_of_measurement=UNIT_OHM,
                icon=ICON_OMEGA,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_IMPEDANCE_LOW): sensor.sensor_schema(
                unit_of_measurement=UNIT_OHM,
                icon=ICON_OMEGA,
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HEART_RATE): sensor.sensor_schema(
                unit_of_measurement=UNIT_BEATS_PER_MINUTE,
                icon=ICON_HEART_PULSE,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TIMESTAMP): sensor.sensor_schema(
                icon="mdi:calendar-clock",
                device_class=DEVICE_CLASS_TIMESTAMP,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PROFILE_ID): sensor.sensor_schema(
                icon="mdi:identifier",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ALLOWED_PROFILE_IDS, default=["any"]): cv.ensure_list(
                _validate_profile_ids
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
    cg.add(var.set_clear_impedance(config[CONF_CLEAR_IMPEDANCE]))

    if bindkey := config.get(CONF_BINDKEY):
        cg.add(var.set_bindkey(bindkey))
    if weight_config := config.get(CONF_WEIGHT):
        sens = await sensor.new_sensor(weight_config)
        cg.add(var.set_weight(sens))
    if impedance_config := config.get(CONF_IMPEDANCE):
        sens = await sensor.new_sensor(impedance_config)
        cg.add(var.set_impedance(sens))
    if impedance_low_config := config.get(CONF_IMPEDANCE_LOW):
        sens = await sensor.new_sensor(impedance_low_config)
        cg.add(var.set_impedance_low(sens))
    if heart_rate_config := config.get(CONF_HEART_RATE):
        sens = await sensor.new_sensor(heart_rate_config)
        cg.add(var.set_heart_rate(sens))
    if timestamp_config := config.get(CONF_TIMESTAMP):
        sens = await sensor.new_sensor(timestamp_config)
        cg.add(var.set_timestamp(sens))
    if profile_id_config := config.get(CONF_PROFILE_ID):
        sens = await sensor.new_sensor(profile_id_config)
        cg.add(var.set_profile_id(sens))
    if (
        allowed_profile_ids := config.get(CONF_ALLOWED_PROFILE_IDS)
    ) and "any" not in allowed_profile_ids:
        cg.add(var.set_allow_any_profile_id(False))
        for allowed_id in allowed_profile_ids:
            cg.add(var.add_allowed_profile_id(allowed_id))
