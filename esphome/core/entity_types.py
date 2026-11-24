"""The entity platforms of esphome/core/entity_types.h, for code generation."""

# Every entity platform with an App list, in entity_types.h order (a test keeps the two in sync).
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

# Platforms whose entity_types.h `upper` token and USE_* define carry a DATETIME_ prefix.
DATETIME_SUBTYPES = {"date", "time", "datetime"}
