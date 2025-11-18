import esphome.codegen as cg
from esphome.components import binary_sensor, esp32_ble_tracker
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINARY_SENSORS,
    CONF_BINDKEY,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_TYPE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_COLD,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_DOOR,
    DEVICE_CLASS_GARAGE_DOOR,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_LIGHT,
    DEVICE_CLASS_LOCK,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_MOVING,
    DEVICE_CLASS_OCCUPANCY,
    DEVICE_CLASS_OPENING,
    DEVICE_CLASS_PLUG,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PRESENCE,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_RUNNING,
    DEVICE_CLASS_SAFETY,
    DEVICE_CLASS_SMOKE,
    DEVICE_CLASS_SOUND,
    DEVICE_CLASS_TAMPER,
    DEVICE_CLASS_VIBRATION,
    DEVICE_CLASS_WINDOW,
)

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["esp32_ble_tracker"]

bthome_ble_ns = cg.esphome_ns.namespace("bthome_ble")
BTHomeBinarySensor = bthome_ble_ns.class_(
    "BTHomeBinarySensor", binary_sensor.BinarySensor, esp32_ble_tracker.ESPBTDeviceListener
)

# BTHome object ID type mapping for binary sensors
BINARY_SENSOR_TYPES = {
    "battery_low": 0x15,
    "battery_charging": 0x16,
    "carbon_monoxide": 0x17,
    "cold": 0x18,
    "connectivity": 0x19,
    "door": 0x1A,
    "garage_door": 0x1B,
    "gas": 0x1C,
    "heat": 0x1D,
    "light": 0x1E,
    "lock": 0x1F,
    "moisture": 0x20,
    "motion": 0x21,
    "moving": 0x22,
    "occupancy": 0x23,
    "opening": 0x24,
    "plug": 0x25,
    "power_on": 0x26,
    "presence": 0x27,
    "problem": 0x28,
    "running": 0x29,
    "safety": 0x2A,
    "smoke": 0x2B,
    "sound": 0x2C,
    "tamper": 0x2D,
    "vibration": 0x2E,
    "window": 0x2F,
}

# Default configurations for binary sensor types
BINARY_SENSOR_DEFAULTS = {
    "battery_low": {"device_class": DEVICE_CLASS_BATTERY},
    "battery_charging": {"device_class": DEVICE_CLASS_BATTERY_CHARGING},
    "cold": {"device_class": DEVICE_CLASS_COLD},
    "connectivity": {"device_class": DEVICE_CLASS_CONNECTIVITY},
    "door": {"device_class": DEVICE_CLASS_DOOR},
    "garage_door": {"device_class": DEVICE_CLASS_GARAGE_DOOR},
    "gas": {"device_class": DEVICE_CLASS_GAS},
    "heat": {"device_class": DEVICE_CLASS_HEAT},
    "light": {"device_class": DEVICE_CLASS_LIGHT},
    "lock": {"device_class": DEVICE_CLASS_LOCK},
    "moisture": {"device_class": DEVICE_CLASS_MOISTURE},
    "motion": {"device_class": DEVICE_CLASS_MOTION},
    "moving": {"device_class": DEVICE_CLASS_MOVING},
    "occupancy": {"device_class": DEVICE_CLASS_OCCUPANCY},
    "opening": {"device_class": DEVICE_CLASS_OPENING},
    "plug": {"device_class": DEVICE_CLASS_PLUG},
    "power_on": {"device_class": DEVICE_CLASS_POWER},
    "presence": {"device_class": DEVICE_CLASS_PRESENCE},
    "problem": {"device_class": DEVICE_CLASS_PROBLEM},
    "running": {"device_class": DEVICE_CLASS_RUNNING},
    "safety": {"device_class": DEVICE_CLASS_SAFETY},
    "smoke": {"device_class": DEVICE_CLASS_SMOKE},
    "sound": {"device_class": DEVICE_CLASS_SOUND},
    "tamper": {"device_class": DEVICE_CLASS_TAMPER},
    "vibration": {"device_class": DEVICE_CLASS_VIBRATION},
    "window": {"device_class": DEVICE_CLASS_WINDOW},
}


def apply_defaults(config):
    """Apply default configurations based on binary sensor type"""
    sensor_type = config[CONF_TYPE]
    if sensor_type in BINARY_SENSOR_DEFAULTS:
        defaults = BINARY_SENSOR_DEFAULTS[sensor_type]
        for key, value in defaults.items():
            if key not in config:
                config[key] = value
    return config


# Schema for individual binary sensors in the list
BINARY_SENSOR_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(BTHomeBinarySensor).extend(
        {
            cv.Required(CONF_TYPE): cv.enum(BINARY_SENSOR_TYPES, lower=True),
        }
    ),
    apply_defaults,
)

# Platform schema with list of binary sensors
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(esp32_ble_tracker.ESP32BLETracker),
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_BINDKEY): cv.bind_key,
        cv.Required(CONF_BINARY_SENSORS): cv.ensure_list(BINARY_SENSOR_SCHEMA),
    }
)


async def to_code(config):
    tracker = await cg.get_variable(config[CONF_ID])

    for sensor_config in config[CONF_BINARY_SENSORS]:
        var = await binary_sensor.new_binary_sensor(sensor_config)
        await esp32_ble_tracker.register_ble_device(var, config)

        object_id = BINARY_SENSOR_TYPES[sensor_config[CONF_TYPE]]
        cg.add(var.set_object_id(object_id))

        if bindkey := config.get(CONF_BINDKEY):
            cg.add(var.set_bindkey(bindkey))
