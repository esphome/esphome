import esphome.codegen as cg
from esphome.components import binary_sensor, esp32_ble, sensor
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.components.esp32_ble import CONF_BLE_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINARY_SENSORS,
    CONF_ID,
    CONF_SENSORS,
    CONF_TX_POWER,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_COLD,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DOOR,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_GARAGE_DOOR,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_LIGHT,
    DEVICE_CLASS_LOCK,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_OCCUPANCY,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_SMOKE,
    DEVICE_CLASS_SPEED,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_TIMESTAMP,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_WEIGHT,
    DEVICE_CLASS_WINDOW,
)
from esphome.core import TimePeriod

AUTO_LOAD = ["esp32_ble"]
DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@esphome/core"]

bthome_ns = cg.esphome_ns.namespace("bthome")
BTHome = bthome_ns.class_(
    "BTHome",
    cg.Component,
    esp32_ble.GAPEventHandler,
    cg.Parented.template(esp32_ble.ESP32BLE),
)

# Configuration constants
CONF_ENCRYPTION_KEY = "encryption_key"
CONF_MIN_INTERVAL = "min_interval"
CONF_MAX_INTERVAL = "max_interval"
CONF_SENSOR_TYPE = "type"
CONF_ADVERTISE_IMMEDIATELY = "advertise_immediately"

# BTHome object IDs for sensors (mapping from device class to BTHome object ID)
SENSOR_DEVICE_CLASS_TO_OBJECT_ID = {
    DEVICE_CLASS_BATTERY: 0x01,
    DEVICE_CLASS_TEMPERATURE: 0x02,  # 0.01°C
    DEVICE_CLASS_HUMIDITY: 0x03,
    DEVICE_CLASS_PRESSURE: 0x04,
    DEVICE_CLASS_ILLUMINANCE: 0x05,
    DEVICE_CLASS_WEIGHT: 0x06,  # mass in kg
    "dewpoint": 0x08,  # Not a standard device class
    DEVICE_CLASS_ENERGY: 0x0A,
    DEVICE_CLASS_POWER: 0x0B,
    DEVICE_CLASS_VOLTAGE: 0x0C,
    DEVICE_CLASS_PM25: 0x0D,
    DEVICE_CLASS_PM10: 0x0E,
    DEVICE_CLASS_CARBON_DIOXIDE: 0x12,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS: 0x13,  # TVOC
    DEVICE_CLASS_MOISTURE: 0x14,
    DEVICE_CLASS_CURRENT: 0x43,
    DEVICE_CLASS_SPEED: 0x44,
    DEVICE_CLASS_TIMESTAMP: 0x50,
}

# BTHome object IDs for binary sensors (mapping from device class to BTHome object ID)
BINARY_SENSOR_DEVICE_CLASS_TO_OBJECT_ID = {
    DEVICE_CLASS_EMPTY: 0x0F,  # generic boolean
    "power": 0x10,  # power state (not DEVICE_CLASS_POWER which is for sensors)
    "battery_low": 0x15,  # Not a standard device class
    DEVICE_CLASS_BATTERY_CHARGING: 0x16,
    DEVICE_CLASS_CARBON_MONOXIDE: 0x17,
    DEVICE_CLASS_COLD: 0x18,
    DEVICE_CLASS_DOOR: 0x1A,
    DEVICE_CLASS_GARAGE_DOOR: 0x1B,
    DEVICE_CLASS_LIGHT: 0x1E,
    DEVICE_CLASS_LOCK: 0x1F,
    DEVICE_CLASS_MOTION: 0x21,
    DEVICE_CLASS_OCCUPANCY: 0x23,
    DEVICE_CLASS_SMOKE: 0x29,
    DEVICE_CLASS_WINDOW: 0x2D,
}


def validate_config(config):
    if config[CONF_MIN_INTERVAL] > config.get(CONF_MAX_INTERVAL):
        raise cv.Invalid("min_interval must be <= max_interval")
    return config


def validate_encryption_key(value):
    value = cv.string_strict(value)
    # Remove any spaces or dashes
    value = value.replace(" ", "").replace("-", "")
    # Must be 32 hex characters (16 bytes)
    if len(value) != 32:
        raise cv.Invalid("Encryption key must be 32 hexadecimal characters (16 bytes)")
    try:
        int(value, 16)
    except ValueError as e:
        raise cv.Invalid("Encryption key must be valid hexadecimal") from e
    return value.lower()


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BTHome),
            cv.GenerateID(CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
            cv.Optional(CONF_MIN_INTERVAL, default="1s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=TimePeriod(milliseconds=20), max=TimePeriod(milliseconds=10240)
                ),
            ),
            cv.Optional(CONF_MAX_INTERVAL, default="1s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=TimePeriod(milliseconds=20), max=TimePeriod(milliseconds=10240)
                ),
            ),
            cv.Optional(CONF_TX_POWER, default="3dBm"): cv.All(
                cv.decibel, cv.enum(esp32_ble.TX_POWER_LEVELS, int=True)
            ),
            cv.Optional(CONF_ENCRYPTION_KEY): validate_encryption_key,
            cv.Optional(CONF_SENSORS): cv.ensure_list(
                cv.Schema(
                    {
                        cv.Required(CONF_SENSOR_TYPE): cv.one_of(
                            *SENSOR_DEVICE_CLASS_TO_OBJECT_ID.keys(), lower=True
                        ),
                        cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
                        cv.Optional(
                            CONF_ADVERTISE_IMMEDIATELY, default=False
                        ): cv.boolean,
                    }
                )
            ),
            cv.Optional(CONF_BINARY_SENSORS): cv.ensure_list(
                cv.Schema(
                    {
                        cv.Required(CONF_SENSOR_TYPE): cv.one_of(
                            *BINARY_SENSOR_DEVICE_CLASS_TO_OBJECT_ID.keys(), lower=True
                        ),
                        cv.Required(CONF_ID): cv.use_id(binary_sensor.BinarySensor),
                        cv.Optional(
                            CONF_ADVERTISE_IMMEDIATELY, default=False
                        ): cv.boolean,
                    }
                )
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_config,
)

FINAL_VALIDATE_SCHEMA = esp32_ble.validate_variant


async def to_code(config):
    # Calculate sizes for StaticVector compile-time allocation
    num_sensors = max(1, len(config.get(CONF_SENSORS, [])))
    num_binary_sensors = max(1, len(config.get(CONF_BINARY_SENSORS, [])))
    max_packets = max(1, num_sensors + num_binary_sensors)

    # Add defines for compile-time sizes (must be before new_Pvariable)
    cg.add_define("BTHOME_MAX_MEASUREMENTS", num_sensors)
    cg.add_define("BTHOME_MAX_BINARY_MEASUREMENTS", num_binary_sensors)
    cg.add_define("BTHOME_MAX_ADV_PACKETS", max_packets)

    var = cg.new_Pvariable(config[CONF_ID])

    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    esp32_ble.register_gap_event_handler(parent, var)

    await cg.register_component(var, config)
    cg.add(var.set_min_interval(config[CONF_MIN_INTERVAL]))
    cg.add(var.set_max_interval(config[CONF_MAX_INTERVAL]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))

    if CONF_ENCRYPTION_KEY in config:
        key = config[CONF_ENCRYPTION_KEY]
        key_bytes = [
            cg.RawExpression(f"0x{key[i : i + 2]}") for i in range(0, len(key), 2)
        ]
        key_array = cg.RawExpression(
            f"std::array<uint8_t, 16>{{{', '.join(str(b) for b in key_bytes)}}}"
        )
        cg.add(var.set_encryption_key(key_array))

    # Add sensor measurements
    if CONF_SENSORS in config:
        for measurement in config[CONF_SENSORS]:
            sensor_type = measurement[CONF_SENSOR_TYPE]
            object_id = SENSOR_DEVICE_CLASS_TO_OBJECT_ID[sensor_type]
            sens = await cg.get_variable(measurement[CONF_ID])
            advertise_immediately = measurement[CONF_ADVERTISE_IMMEDIATELY]
            cg.add(var.add_measurement(sens, object_id, advertise_immediately))

    # Add binary sensor measurements
    if CONF_BINARY_SENSORS in config:
        for measurement in config[CONF_BINARY_SENSORS]:
            sensor_type = measurement[CONF_SENSOR_TYPE]
            object_id = BINARY_SENSOR_DEVICE_CLASS_TO_OBJECT_ID[sensor_type]
            sens = await cg.get_variable(measurement[CONF_ID])
            advertise_immediately = measurement[CONF_ADVERTISE_IMMEDIATELY]
            cg.add(var.add_binary_measurement(sens, object_id, advertise_immediately))

    cg.add_define("USE_ESP32_BLE_ADVERTISING")

    add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_BLE_42_FEATURES_SUPPORTED", True)
