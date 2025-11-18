import esphome.codegen as cg
from esphome.components import sensor, esp32_ble_tracker
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINDKEY,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_SENSORS,
    CONF_TYPE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CO2,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_KILOGRAM,
    UNIT_KILOWATT_HOURS,
    UNIT_LUX,
    UNIT_METER,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_MILLIMETER,
    UNIT_PARTS_PER_MILLION,
    UNIT_PASCAL,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_WATT,
)

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["esp32_ble_tracker"]

bthome_ble_ns = cg.esphome_ns.namespace("bthome_ble")
BTHomeSensor = bthome_ble_ns.class_(
    "BTHomeSensor", sensor.Sensor, esp32_ble_tracker.ESPBTDeviceListener
)

# BTHome object ID type mapping
SENSOR_TYPES = {
    "packet_id": 0x00,
    "battery": 0x01,
    "temperature": 0x02,
    "humidity": 0x03,
    "pressure": 0x04,
    "illuminance": 0x05,
    "mass_kg": 0x06,
    "mass_lb": 0x07,
    "dewpoint": 0x08,
    "count": 0x09,
    "energy": 0x0A,
    "power": 0x0B,
    "voltage": 0x0C,
    "pm25": 0x0D,
    "pm10": 0x0E,
    "co2": 0x12,
    "voc": 0x13,
    "moisture": 0x14,
    "count_uint16": 0x3D,
    "count_uint32": 0x3E,
    "rotation": 0x3F,
    "distance_mm": 0x40,
    "distance_m": 0x41,
    "duration": 0x42,
    "current": 0x43,
    "speed": 0x44,
    "temperature_precise": 0x45,
    "uv_index": 0x46,
    "volume_l": 0x47,
    "volume_ml": 0x48,
    "volume_flow": 0x49,
    "voltage_precise": 0x4A,
    "gas_volume": 0x4B,
    "gas_volume_l": 0x4C,
    "energy_precise": 0x4D,
    "volume_precise": 0x4E,
    "water": 0x4F,
    "timestamp": 0x50,
    "acceleration": 0x51,
    "gyroscope": 0x52,
}

# Default configurations for sensor types
SENSOR_DEFAULTS = {
    "battery": {
        "unit_of_measurement": UNIT_PERCENT,
        "accuracy_decimals": 0,
        "device_class": DEVICE_CLASS_BATTERY,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "temperature": {
        "unit_of_measurement": UNIT_CELSIUS,
        "accuracy_decimals": 2,
        "device_class": DEVICE_CLASS_TEMPERATURE,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "temperature_precise": {
        "unit_of_measurement": UNIT_CELSIUS,
        "accuracy_decimals": 1,
        "device_class": DEVICE_CLASS_TEMPERATURE,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "humidity": {
        "unit_of_measurement": UNIT_PERCENT,
        "accuracy_decimals": 2,
        "device_class": DEVICE_CLASS_HUMIDITY,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "pressure": {
        "unit_of_measurement": UNIT_PASCAL,
        "accuracy_decimals": 2,
        "device_class": DEVICE_CLASS_PRESSURE,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "illuminance": {
        "unit_of_measurement": UNIT_LUX,
        "accuracy_decimals": 2,
        "device_class": DEVICE_CLASS_ILLUMINANCE,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "mass_kg": {
        "unit_of_measurement": UNIT_KILOGRAM,
        "accuracy_decimals": 2,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "mass_lb": {
        "unit_of_measurement": "lb",
        "accuracy_decimals": 2,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "dewpoint": {
        "unit_of_measurement": UNIT_CELSIUS,
        "accuracy_decimals": 2,
        "device_class": DEVICE_CLASS_TEMPERATURE,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "count": {
        "accuracy_decimals": 0,
        "state_class": STATE_CLASS_TOTAL_INCREASING,
    },
    "energy": {
        "unit_of_measurement": UNIT_KILOWATT_HOURS,
        "accuracy_decimals": 3,
        "device_class": DEVICE_CLASS_ENERGY,
        "state_class": STATE_CLASS_TOTAL_INCREASING,
    },
    "power": {
        "unit_of_measurement": UNIT_WATT,
        "accuracy_decimals": 2,
        "device_class": DEVICE_CLASS_POWER,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "voltage": {
        "unit_of_measurement": UNIT_VOLT,
        "accuracy_decimals": 3,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "voltage_precise": {
        "unit_of_measurement": UNIT_VOLT,
        "accuracy_decimals": 1,
        "device_class": DEVICE_CLASS_VOLTAGE,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "pm25": {
        "unit_of_measurement": UNIT_MICROGRAMS_PER_CUBIC_METER,
        "accuracy_decimals": 0,
        "device_class": DEVICE_CLASS_PM25,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "pm10": {
        "unit_of_measurement": UNIT_MICROGRAMS_PER_CUBIC_METER,
        "accuracy_decimals": 0,
        "device_class": DEVICE_CLASS_PM10,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "co2": {
        "unit_of_measurement": UNIT_PARTS_PER_MILLION,
        "accuracy_decimals": 0,
        "device_class": DEVICE_CLASS_CO2,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "voc": {
        "unit_of_measurement": UNIT_MICROGRAMS_PER_CUBIC_METER,
        "accuracy_decimals": 0,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "moisture": {
        "unit_of_measurement": UNIT_PERCENT,
        "accuracy_decimals": 2,
        "device_class": DEVICE_CLASS_MOISTURE,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "current": {
        "unit_of_measurement": UNIT_AMPERE,
        "accuracy_decimals": 3,
        "device_class": DEVICE_CLASS_CURRENT,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "distance_mm": {
        "unit_of_measurement": UNIT_MILLIMETER,
        "accuracy_decimals": 0,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
    "distance_m": {
        "unit_of_measurement": UNIT_METER,
        "accuracy_decimals": 1,
        "state_class": STATE_CLASS_MEASUREMENT,
    },
}


def apply_defaults(config):
    """Apply default configurations based on sensor type"""
    sensor_type = config[CONF_TYPE]
    if sensor_type in SENSOR_DEFAULTS:
        defaults = SENSOR_DEFAULTS[sensor_type]
        for key, value in defaults.items():
            if key not in config:
                config[key] = value
    return config


# Schema for individual sensors in the list
SENSOR_SCHEMA = cv.All(
    sensor.sensor_schema(BTHomeSensor).extend(
        {
            cv.Required(CONF_TYPE): cv.enum(SENSOR_TYPES, lower=True),
        }
    ),
    apply_defaults,
)

# Platform schema with list of sensors
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(esp32_ble_tracker.ESP32BLETracker),
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_BINDKEY): cv.bind_key,
        cv.Required(CONF_SENSORS): cv.ensure_list(SENSOR_SCHEMA),
    }
)


async def to_code(config):
    tracker = await cg.get_variable(config[CONF_ID])

    for sensor_config in config[CONF_SENSORS]:
        var = await sensor.new_sensor(sensor_config)
        await esp32_ble_tracker.register_ble_device(var, config)

        object_id = SENSOR_TYPES[sensor_config[CONF_TYPE]]
        cg.add(var.set_object_id(object_id))

        if bindkey := config.get(CONF_BINDKEY):
            cg.add(var.set_bindkey(bindkey))
