"""Micronova constants."""

from dataclasses import dataclass
from typing import TypedDict


@dataclass(frozen=True, slots=True)
class MicronovaMemory:
    """Micronova memory."""

    location: int
    """The memory location."""

    address: int
    """The memory address."""


# Number entities
CONF_THERMOSTAT_TEMPERATURE = "thermostat_temperature"
CONF_POWER_LEVEL = "power_level"

# Sensor entities
CONF_FAN_SPEED = "fan_speed"
CONF_FUMES_TEMPERATURE = "fumes_temperature"
CONF_ROOM_TEMPERATURE = "room_temperature"
CONF_STOVE_POWER = "stove_power"
CONF_WATER_TEMPERATURE = "water_temperature"
CONF_WATER_PRESSURE = "water_pressure"

# Switch entities
CONF_STOVE = "stove"

# Text sensor entities
CONF_STOVE_STATE = "stove_state"


class ModelConfig(TypedDict, total=False):
    """Model configuration with memory addresses for each entity."""

    # Number entities
    thermostat_temperature: MicronovaMemory
    power_level: MicronovaMemory
    # Sensor entities
    fan_speed: MicronovaMemory
    fumes_temperature: MicronovaMemory
    room_temperature: MicronovaMemory
    stove_power: MicronovaMemory
    water_temperature: MicronovaMemory
    water_pressure: MicronovaMemory
    # Switch entities
    stove: MicronovaMemory
    # Text sensor entities
    stove_state: MicronovaMemory
