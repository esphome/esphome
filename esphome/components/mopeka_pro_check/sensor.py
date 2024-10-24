import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import (
    CONF_DISTANCE,
    CONF_MAC_ADDRESS,
    CONF_ID,
    ICON_COUNTER,
    ICON_THERMOMETER,
    ICON_RULER,
    ICON_SIGNAL,
    UNIT_PERCENT,
    UNIT_EMPTY,
    CONF_LEVEL,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,
    UNIT_MILLIMETER,
    STATE_CLASS_MEASUREMENT,
    CONF_BATTERY_LEVEL,
    DEVICE_CLASS_BATTERY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

CONF_TANK_TYPE = "tank_type"
CONF_CUSTOM_DISTANCE_FULL = "custom_distance_full"
CONF_CUSTOM_DISTANCE_EMPTY = "custom_distance_empty"
CONF_SIGNAL_QUALITY = "signal_quality"
CONF_MINIMUM_SIGNAL_QUALITY = "minimum_signal_quality"
CONF_IGNORED_READS = "ignored_reads"

ICON_PROPANE_TANK = "mdi:propane-tank"

TANK_TYPE_CUSTOM = "CUSTOM"


def small_distance(value):
    """small_distance is stored in mm"""
    meters = cv.distance(value)
    return meters * 1000


#
# Map of standard tank types to their
# empty and full distance values.
# Format is - tank name: (empty distance in mm, full distance in mm)
#
CONF_SUPPORTED_TANKS_MAP = {
    TANK_TYPE_CUSTOM: (0, 100),
    "20LB_V": (38, 254),  # empty/full readings for 20lb US tank
    "30LB_V": (38, 381),
    "40LB_V": (38, 508),
    "EUROPE_6KG": (38, 336),
    "EUROPE_11KG": (38, 366),
    "EUROPE_14KG": (38, 467),
}

CODEOWNERS = ["@spbrogan"]
DEPENDENCIES = ["esp32_ble_tracker"]

mopeka_pro_check_ns = cg.esphome_ns.namespace("mopeka_pro_check")
MopekaProCheck = mopeka_pro_check_ns.class_(
    "MopekaProCheck", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

SensorReadQuality = mopeka_pro_check_ns.enum("SensorReadQuality")
SIGNAL_QUALITIES = {
    "ZERO": SensorReadQuality.QUALITY_ZERO,
    "LOW": SensorReadQuality.QUALITY_LOW,
    "MEDIUM": SensorReadQuality.QUALITY_MED,
    "HIGH": SensorReadQuality.QUALITY_HIGH,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MopekaProCheck),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_CUSTOM_DISTANCE_FULL): small_distance,
            cv.Optional(CONF_CUSTOM_DISTANCE_EMPTY): small_distance,
            cv.Required(CONF_TANK_TYPE): cv.enum(CONF_SUPPORTED_TANKS_MAP, upper=True),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_PROPANE_TANK,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIMETER,
                icon=ICON_RULER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SIGNAL_QUALITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_SIGNAL,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_IGNORED_READS): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                icon=ICON_COUNTER,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_MINIMUM_SIGNAL_QUALITY, default="MEDIUM"): cv.enum(
                SIGNAL_QUALITIES, upper=True
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

    if config[CONF_TANK_TYPE] == TANK_TYPE_CUSTOM:
        # Support custom tank min/max
        if CONF_CUSTOM_DISTANCE_EMPTY in config:
            cg.add(var.set_tank_empty(config[CONF_CUSTOM_DISTANCE_EMPTY]))
        else:
            cg.add(var.set_tank_empty(CONF_SUPPORTED_TANKS_MAP[TANK_TYPE_CUSTOM][0]))
        if CONF_CUSTOM_DISTANCE_FULL in config:
            cg.add(var.set_tank_full(config[CONF_CUSTOM_DISTANCE_FULL]))
        else:
            cg.add(var.set_tank_full(CONF_SUPPORTED_TANKS_MAP[TANK_TYPE_CUSTOM][1]))
    else:
        # Set the Tank empty and full based on map - User is requesting standard tank
        t = config[CONF_TANK_TYPE]
        cg.add(var.set_tank_empty(CONF_SUPPORTED_TANKS_MAP[t][0]))
        cg.add(var.set_tank_full(CONF_SUPPORTED_TANKS_MAP[t][1]))

    if (
        minimum_signal_quality := config.get(CONF_MINIMUM_SIGNAL_QUALITY, None)
    ) is not None:
        cg.add(var.set_min_signal_quality(minimum_signal_quality))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))
    if CONF_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_LEVEL])
        cg.add(var.set_level(sens))
    if CONF_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_DISTANCE])
        cg.add(var.set_distance(sens))
    if CONF_BATTERY_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_level(sens))
    if CONF_SIGNAL_QUALITY in config:
        sens = await sensor.new_sensor(config[CONF_SIGNAL_QUALITY])
        cg.add(var.set_signal_quality(sens))
    if CONF_IGNORED_READS in config:
        sens = await sensor.new_sensor(config[CONF_IGNORED_READS])
        cg.add(var.set_ignored_reads(sens))
