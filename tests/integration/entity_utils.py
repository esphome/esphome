"""Utilities for computing entity object_id in integration tests.

This module contains the algorithm that aioesphomeapi will use to compute
object_id client-side from API data.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from esphome.helpers import fnv1_hash_object_id, sanitize, snake_case

if TYPE_CHECKING:
    from aioesphomeapi import DeviceInfo, EntityInfo


def compute_object_id(name: str) -> str:
    """Compute object_id from name using snake_case + sanitize."""
    return sanitize(snake_case(name))


def infer_name_add_mac_suffix(device_info: DeviceInfo) -> bool:
    """Infer name_add_mac_suffix from device name ending with MAC suffix."""
    mac_suffix = device_info.mac_address.replace(":", "")[-6:].lower()
    return device_info.name.endswith(f"-{mac_suffix}")


def _get_name_for_object_id(
    entity: EntityInfo,
    device_info: DeviceInfo,
    device_id_to_name: dict[int, str],
) -> str:
    """Get the name used for object_id computation.

    This is the algorithm that aioesphomeapi will use to determine which
    name to use for computing object_id client-side from API data.

    Args:
        entity: The entity to get name for
        device_info: Device info from the API
        device_id_to_name: Mapping of device_id to device name for sub-devices

    Returns:
        The name to use for object_id computation
    """
    if entity.name:
        # Named entity: use entity name
        return entity.name
    if entity.device_id != 0:
        # Empty name on sub-device: use sub-device name
        return device_id_to_name[entity.device_id]
    if infer_name_add_mac_suffix(device_info) or device_info.friendly_name:
        # Empty name on main device with MAC suffix or friendly_name: use friendly_name
        # (even if empty - this is bug-for-bug compatibility for MAC suffix case)
        return device_info.friendly_name
    # Empty name on main device, no friendly_name: use device name
    return device_info.name


def compute_entity_object_id(
    entity: EntityInfo,
    device_info: DeviceInfo,
    device_id_to_name: dict[int, str],
) -> str:
    """Compute expected object_id for an entity.

    Args:
        entity: The entity to compute object_id for
        device_info: Device info from the API
        device_id_to_name: Mapping of device_id to device name for sub-devices

    Returns:
        The computed object_id string
    """
    name_for_id = _get_name_for_object_id(entity, device_info, device_id_to_name)
    return compute_object_id(name_for_id)


def compute_entity_hash(
    entity: EntityInfo,
    device_info: DeviceInfo,
    device_id_to_name: dict[int, str],
) -> int:
    """Compute expected object_id hash for an entity.

    Args:
        entity: The entity to compute hash for
        device_info: Device info from the API
        device_id_to_name: Mapping of device_id to device name for sub-devices

    Returns:
        The computed FNV-1 hash
    """
    name_for_id = _get_name_for_object_id(entity, device_info, device_id_to_name)
    return fnv1_hash_object_id(name_for_id)


def verify_entity_object_id(
    entity: EntityInfo,
    device_info: DeviceInfo,
    device_id_to_name: dict[int, str],
) -> None:
    """Verify an entity's object_id and hash match the expected values.

    Args:
        entity: The entity to verify
        device_info: Device info from the API
        device_id_to_name: Mapping of device_id to device name for sub-devices

    Raises:
        AssertionError: If object_id or hash doesn't match expected value
    """
    expected_object_id = compute_entity_object_id(
        entity, device_info, device_id_to_name
    )
    assert entity.object_id == expected_object_id, (
        f"object_id mismatch for entity '{entity.name}': "
        f"expected '{expected_object_id}', got '{entity.object_id}'"
    )

    expected_hash = compute_entity_hash(entity, device_info, device_id_to_name)
    assert entity.key == expected_hash, (
        f"hash mismatch for entity '{entity.name}': "
        f"expected {expected_hash:#x}, got {entity.key:#x}"
    )


def verify_all_entities(
    entities: list[EntityInfo],
    device_info: DeviceInfo,
) -> None:
    """Verify all entities have correct object_id and hash values.

    Args:
        entities: List of entities to verify
        device_info: Device info from the API

    Raises:
        AssertionError: If any entity's object_id or hash doesn't match
    """
    # Build device_id -> name lookup from sub-devices
    device_id_to_name = {d.device_id: d.name for d in device_info.devices}

    for entity in entities:
        verify_entity_object_id(entity, device_info, device_id_to_name)
