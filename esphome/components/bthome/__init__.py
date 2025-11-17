import esphome.codegen as cg
from esphome.components import binary_sensor, esp32_ble, sensor
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.components.esp32_ble import CONF_BLE_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_BATTERY_LEVEL,
    CONF_HUMIDITY,
    CONF_ILLUMINANCE,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    CONF_TX_POWER,
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
CONF_MEASUREMENT = "measurement"
CONF_ENCRYPTION_KEY = "encryption_key"
CONF_MIN_INTERVAL = "min_interval"
CONF_MAX_INTERVAL = "max_interval"
CONF_SENSOR_TYPE = "type"

# Sensor type constants
CONF_BATTERY = "battery"
CONF_CO2 = "co2"
CONF_COUNT = "count"
CONF_CURRENT = "current"
CONF_DEWPOINT = "dewpoint"
CONF_DISTANCE = "distance"
CONF_ENERGY = "energy"
CONF_GAS = "gas"
CONF_MASS = "mass"
CONF_MOISTURE = "moisture"
CONF_PM25 = "pm2_5"
CONF_PM10 = "pm10"
CONF_POWER = "power"
CONF_ROTATION = "rotation"
CONF_SPEED = "speed"
CONF_TIMESTAMP = "timestamp"
CONF_TVOC = "tvoc"
CONF_VOLTAGE = "voltage"
CONF_VOLUME = "volume"

# Binary sensor types
CONF_BINARY_SENSOR = "binary_sensor"
CONF_BATTERY_LOW = "battery_low"
CONF_BATTERY_CHARGING = "battery_charging"
CONF_CO = "carbon_monoxide"
CONF_COLD = "cold"
CONF_CONNECTIVITY = "connectivity"
CONF_DOOR = "door"
CONF_GARAGE_DOOR = "garage_door"
CONF_GAS_DETECTED = "gas"
CONF_GENERIC_BOOLEAN = "generic_boolean"
CONF_HEAT = "heat"
CONF_LIGHT = "light"
CONF_LOCK = "lock"
CONF_MOISTURE_DETECTED = "moisture"
CONF_MOTION = "motion"
CONF_MOVING = "moving"
CONF_OCCUPANCY = "occupancy"
CONF_OPENING = "opening"
CONF_PLUG = "plug"
CONF_POWER_ON = "power"
CONF_PRESENCE = "presence"
CONF_PROBLEM = "problem"
CONF_RUNNING = "running"
CONF_SAFETY = "safety"
CONF_SMOKE = "smoke"
CONF_SOUND = "sound"
CONF_TAMPER = "tamper"
CONF_VIBRATION = "vibration"
CONF_WINDOW = "window"

# BTHome object IDs for sensors
SENSOR_OBJECT_IDS = {
    CONF_BATTERY: 0x01,
    CONF_TEMPERATURE: 0x02,  # 0.01°C
    CONF_HUMIDITY: 0x03,
    CONF_PRESSURE: 0x04,
    CONF_ILLUMINANCE: 0x05,
    CONF_MASS: 0x06,
    CONF_DEWPOINT: 0x08,
    CONF_ENERGY: 0x0A,
    CONF_POWER: 0x0B,
    CONF_VOLTAGE: 0x0C,
    CONF_PM25: 0x0D,
    CONF_PM10: 0x0E,
    CONF_CO2: 0x12,
    CONF_TVOC: 0x13,
    CONF_MOISTURE: 0x14,
    CONF_CURRENT: 0x43,
    CONF_SPEED: 0x44,
    CONF_TIMESTAMP: 0x50,
}

# BTHome object IDs for binary sensors
BINARY_SENSOR_OBJECT_IDS = {
    CONF_GENERIC_BOOLEAN: 0x0F,
    CONF_POWER_ON: 0x10,
    CONF_BATTERY_LOW: 0x15,
    CONF_BATTERY_CHARGING: 0x16,
    CONF_CO: 0x17,
    CONF_COLD: 0x18,
    CONF_DOOR: 0x1A,
    CONF_GARAGE_DOOR: 0x1B,
    CONF_LIGHT: 0x1E,
    CONF_LOCK: 0x1F,
    CONF_MOTION: 0x21,
    CONF_OCCUPANCY: 0x23,
    CONF_SMOKE: 0x29,
    CONF_WINDOW: 0x2D,
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
            cv.Optional(CONF_MEASUREMENT): cv.ensure_list(
                cv.Schema(
                    {
                        cv.Required(CONF_SENSOR_TYPE): cv.one_of(
                            *SENSOR_OBJECT_IDS.keys(), lower=True
                        ),
                        cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
                    }
                )
            ),
            cv.Optional(CONF_BINARY_SENSOR): cv.ensure_list(
                cv.Schema(
                    {
                        cv.Required(CONF_SENSOR_TYPE): cv.one_of(
                            *BINARY_SENSOR_OBJECT_IDS.keys(), lower=True
                        ),
                        cv.Required(CONF_ID): cv.use_id(binary_sensor.BinarySensor),
                    }
                )
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_config,
)

FINAL_VALIDATE_SCHEMA = esp32_ble.validate_variant


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    esp32_ble.register_gap_event_handler(parent, var)

    await cg.register_component(var, config)
    cg.add(var.set_min_interval(config[CONF_MIN_INTERVAL]))
    cg.add(var.set_max_interval(config[CONF_MAX_INTERVAL]))
    cg.add(var.set_tx_power(config[CONF_TX_POWER]))

    if CONF_ENCRYPTION_KEY in config:
        key = config[CONF_ENCRYPTION_KEY]
        key_bytes = [int(key[i : i + 2], 16) for i in range(0, len(key), 2)]
        cg.add(var.set_encryption_key(key_bytes))

    # Add sensor measurements
    if CONF_MEASUREMENT in config:
        for measurement in config[CONF_MEASUREMENT]:
            sensor_type = measurement[CONF_SENSOR_TYPE]
            object_id = SENSOR_OBJECT_IDS[sensor_type]
            sens = await cg.get_variable(measurement[CONF_ID])
            cg.add(var.add_measurement(sens, object_id))

    # Add binary sensor measurements
    if CONF_BINARY_SENSOR in config:
        for measurement in config[CONF_BINARY_SENSOR]:
            sensor_type = measurement[CONF_SENSOR_TYPE]
            object_id = BINARY_SENSOR_OBJECT_IDS[sensor_type]
            sens = await cg.get_variable(measurement[CONF_ID])
            cg.add(var.add_binary_measurement(sens, object_id))

    cg.add_define("USE_ESP32_BLE_ADVERTISING")

    add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_BLE_42_FEATURES_SUPPORTED", True)
