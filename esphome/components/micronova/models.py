"""Micronova models."""

from .const import (
    CONF_FAN_SPEED,
    CONF_FUMES_TEMPERATURE,
    CONF_POWER_LEVEL,
    CONF_ROOM_TEMPERATURE,
    CONF_STOVE,
    CONF_STOVE_POWER,
    CONF_STOVE_STATE,
    CONF_THERMOSTAT_TEMPERATURE,
    CONF_WATER_PRESSURE,
    CONF_WATER_TEMPERATURE,
    MicronovaMemory,
    ModelConfig,
)

DEFAULT_MODEL = "extraflame_ketty_evo_2.0"

MODELS: dict[str, ModelConfig] = {
    DEFAULT_MODEL: {
        # Number entities
        CONF_POWER_LEVEL: MicronovaMemory(location=0x20, address=0x7F),
        CONF_THERMOSTAT_TEMPERATURE: MicronovaMemory(location=0x20, address=0x7D),
        # Sensor entities
        CONF_FAN_SPEED: MicronovaMemory(location=0x00, address=0x37),
        CONF_FUMES_TEMPERATURE: MicronovaMemory(location=0x00, address=0x5A),
        CONF_ROOM_TEMPERATURE: MicronovaMemory(location=0x00, address=0x01),
        CONF_STOVE_POWER: MicronovaMemory(location=0x00, address=0x34),
        CONF_WATER_TEMPERATURE: MicronovaMemory(location=0x00, address=0x3B),
        CONF_WATER_PRESSURE: MicronovaMemory(location=0x00, address=0x3C),
        # Switch entities
        CONF_STOVE: MicronovaMemory(location=0x00, address=0x21),
        # Text sensor entities
        CONF_STOVE_STATE: MicronovaMemory(location=0x00, address=0x21),
    },
    "extraflame_preziosa": {
        # Sensor entities
        CONF_FUMES_TEMPERATURE: MicronovaMemory(location=0x00, address=0x02),
        CONF_ROOM_TEMPERATURE: MicronovaMemory(location=0x00, address=0x01),
        # Switch entities
        CONF_STOVE: MicronovaMemory(location=0x00, address=0x21),
        # Text sensor entities
        CONF_STOVE_STATE: MicronovaMemory(location=0x00, address=0x21),
    },
}


def get_model_defaults(model_name: str, entity_key: str) -> MicronovaMemory | None:
    """Get the default memory address for an entity from the model configuration.

    Args:
        model_name: The model name from configuration.
        entity_key: The entity key (e.g., CONF_THERMOSTAT_TEMPERATURE).

    Returns:
        The MicronovaMemory defaults if found, None otherwise.
    """
    return MODELS.get(model_name, {}).get(entity_key)
