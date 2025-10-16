"""Shared utilities for sensor integration tests."""

from __future__ import annotations

from aioesphomeapi import EntityInfo


def build_key_to_sensor_mapping(
    entities: list[EntityInfo], sensor_names: list[str]
) -> dict[int, str]:
    """Build a mapping from entity keys to sensor names.

    Args:
        entities: List of entity info objects from the API
        sensor_names: List of sensor names to search for in object_ids

    Returns:
        Dictionary mapping entity keys to sensor names
    """
    key_to_sensor: dict[int, str] = {}
    for entity in entities:
        obj_id = entity.object_id.lower()
        for sensor_name in sensor_names:
            if sensor_name in obj_id:
                key_to_sensor[entity.key] = sensor_name
                break
    return key_to_sensor
