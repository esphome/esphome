"""The entity platforms of esphome/core/entity_types.h, for code generation."""

# test_entity_types.py keeps this in entity_types.h order
ENTITY_PLATFORMS: tuple[str, ...] = (
    "binary_sensor",
    "cover",
    "fan",
    "light",
    "sensor",
    "switch",
    "button",
    "text_sensor",
    "climate",
    "number",
    "date",
    "time",
    "datetime",
    "text",
    "select",
    "lock",
    "valve",
    "media_player",
    "alarm_control_panel",
    "water_heater",
    "infrared",
    "radio_frequency",
    "event",
    "update",
)

DATETIME_SUBTYPES = {"date", "time", "datetime"}
