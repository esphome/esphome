from esphome import const

from ..bthome import BTHomeObjectType, object_types_by_name

BINARY_SENSOR_OBJECT_TYPES = object_types_by_name(
    BTHomeObjectType(object_id=0x0F, name="GENERIC_BOOLEAN"),
    BTHomeObjectType(object_id=0x10, name="POWER_ON"),
    BTHomeObjectType(
        object_id=0x11, name="OPENING_OPEN", device_class=const.DEVICE_CLASS_OPENING
    ),
    BTHomeObjectType(
        object_id=0x15, name="BATTERY_LOW", device_class=const.DEVICE_CLASS_BATTERY
    ),
    BTHomeObjectType(
        object_id=0x16,
        name="BATTERY_CHARGING",
        device_class=const.DEVICE_CLASS_BATTERY_CHARGING,
    ),
    BTHomeObjectType(
        object_id=0x17,
        name="CO_DETECTED",
        device_class=const.DEVICE_CLASS_CARBON_MONOXIDE,
    ),
    BTHomeObjectType(
        object_id=0x18, name="COLD_DETECTED", device_class=const.DEVICE_CLASS_COLD
    ),
    BTHomeObjectType(
        object_id=0x19,
        name="CONNECTIVITY_CONNECTED",
        device_class=const.DEVICE_CLASS_CONNECTIVITY,
    ),
    BTHomeObjectType(
        object_id=0x1A, name="DOOR_OPEN", device_class=const.DEVICE_CLASS_DOOR
    ),
    BTHomeObjectType(
        object_id=0x1B, name="GARAGE_DOOR_OPEN", device_class=const.DEVICE_CLASS_OPENING
    ),
    BTHomeObjectType(
        object_id=0x1C, name="GAS_DETECTED", device_class=const.DEVICE_CLASS_GAS
    ),
    BTHomeObjectType(
        object_id=0x1D, name="HEAT_DETECTED", device_class=const.DEVICE_CLASS_HEAT
    ),
    BTHomeObjectType(
        object_id=0x1E, name="LIGHT_DETECTED", device_class=const.DEVICE_CLASS_LIGHT
    ),
    BTHomeObjectType(
        object_id=0x1F, name="LOCK_UNLOCKED", device_class=const.DEVICE_CLASS_LOCK
    ),
    BTHomeObjectType(
        object_id=0x20, name="MOISTURE_WET", device_class=const.DEVICE_CLASS_MOISTURE
    ),
    BTHomeObjectType(
        object_id=0x21, name="MOTION_DETECTED", device_class=const.DEVICE_CLASS_MOTION
    ),
    BTHomeObjectType(
        object_id=0x22, name="MOVING_ACTIVE", device_class=const.DEVICE_CLASS_MOVING
    ),
    BTHomeObjectType(
        object_id=0x23,
        name="OCCUPANCY_DETECTED",
        device_class=const.DEVICE_CLASS_OCCUPANCY,
    ),
    BTHomeObjectType(
        object_id=0x24, name="PLUG_PLUGGED_IN", device_class=const.DEVICE_CLASS_PLUG
    ),
    BTHomeObjectType(
        object_id=0x25, name="PRESENCE_HOME", device_class=const.DEVICE_CLASS_PRESENCE
    ),
    BTHomeObjectType(
        object_id=0x26, name="PROBLEM_DETECTED", device_class=const.DEVICE_CLASS_PROBLEM
    ),
    BTHomeObjectType(
        object_id=0x27, name="RUNNING_ACTIVE", device_class=const.DEVICE_CLASS_RUNNING
    ),
    BTHomeObjectType(
        object_id=0x28, name="SAFETY_SAFE", device_class=const.DEVICE_CLASS_SAFETY
    ),
    BTHomeObjectType(
        object_id=0x29, name="SMOKE_DETECTED", device_class=const.DEVICE_CLASS_SMOKE
    ),
    BTHomeObjectType(
        object_id=0x2A, name="SOUND_DETECTED", device_class=const.DEVICE_CLASS_SOUND
    ),
    BTHomeObjectType(
        object_id=0x2B, name="TAMPER_ACTIVE", device_class=const.DEVICE_CLASS_TAMPER
    ),
    BTHomeObjectType(
        object_id=0x2C,
        name="VIBRATION_DETECTED",
        device_class=const.DEVICE_CLASS_VIBRATION,
    ),
    BTHomeObjectType(
        object_id=0x2D, name="WINDOW_OPEN", device_class=const.DEVICE_CLASS_WINDOW
    ),
)
