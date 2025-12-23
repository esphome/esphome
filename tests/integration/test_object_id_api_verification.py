"""Integration test to verify object_id from API matches Python computation.

This test verifies a three-way match between:
1. C++ object_id generation (get_object_id_to using to_sanitized_char/to_snake_case_char)
2. C++ hash generation (fnv1_hash_object_id in helpers.h)
3. Python computation (sanitize/snake_case in helpers.py, fnv1_hash_object_id)

The API response contains C++ computed values, so verifying API == Python
implicitly verifies C++ == Python == API for both object_id and hash.

This is important for the planned migration to remove object_id from the API
protocol and have clients (like aioesphomeapi) compute it from the name.
See: https://github.com/esphome/backlog/issues/76

Test cases covered:
- Named entities with various characters (uppercase, special chars, hyphens, etc.)
- Empty-name entities (has_own_name=false, uses device's friendly_name)
- MAC suffix handling (name_add_mac_suffix modifies friendly_name at runtime)
- Both object_id string and hash (key) verification
"""

from __future__ import annotations

import pytest

from esphome.helpers import fnv1_hash_object_id, sanitize, snake_case

from .types import APIClientConnectedFactory, RunCompiledFunction

# Host platform default MAC: 98:35:69:ab:f6:79 -> suffix "abf679"
MAC_SUFFIX = "abf679"


# Expected entities with their own names and expected object_ids
# Format: (entity_name, expected_object_id)
NAMED_ENTITIES = [
    # sensor platform
    ("Temperature Sensor", "temperature_sensor"),
    ("UPPERCASE NAME", "uppercase_name"),
    ("Special!@Chars#", "special__chars_"),
    ("Temp-Sensor", "temp-sensor"),
    ("Temp_Sensor", "temp_sensor"),
    ("Living Room Temperature", "living_room_temperature"),
    # binary_sensor platform
    ("Door Open", "door_open"),
    ("Sensor 123", "sensor_123"),
    # switch platform
    ("My Very Long Switch Name Here", "my_very_long_switch_name_here"),
    # text_sensor platform
    ("123 Start", "123_start"),
]


def compute_expected_object_id(name: str) -> str:
    """Compute expected object_id from name using Python helpers."""
    return sanitize(snake_case(name))


@pytest.mark.asyncio
async def test_object_id_api_verification(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that object_id from API matches Python computation.

    Tests:
    1. Named entities - object_id computed from entity name
    2. Empty-name entities - object_id computed from friendly_name (with MAC suffix)
    3. Hash verification - key can be computed from name
    4. Generic verification - all entities can have object_id computed from API data
    """
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get device info
        device_info = await client.device_info()
        assert device_info is not None

        # Device name should include MAC suffix (hyphen separator)
        assert device_info.name == f"object-id-test-{MAC_SUFFIX}", (
            f"Device name mismatch: got '{device_info.name}'"
        )
        # Friendly name should include MAC suffix (space separator)
        expected_friendly_name = f"Test Device {MAC_SUFFIX}"
        assert device_info.friendly_name == expected_friendly_name, (
            f"Friendly name mismatch: got '{device_info.friendly_name}'"
        )

        # Get all entities
        entities, _ = await client.list_entities_services()

        # Create a map of entity names to entity info
        entity_map = {}
        for entity in entities:
            entity_map[entity.name] = entity

        # === Test 1: Verify each named entity ===
        for entity_name, expected_object_id in NAMED_ENTITIES:
            assert entity_name in entity_map, (
                f"Entity '{entity_name}' not found in API response. "
                f"Available: {list(entity_map.keys())}"
            )

            entity = entity_map[entity_name]

            # Verify object_id matches expected
            assert entity.object_id == expected_object_id, (
                f"Entity '{entity_name}': object_id mismatch. "
                f"API returned '{entity.object_id}', expected '{expected_object_id}'"
            )

            # Verify Python computation matches
            computed = compute_expected_object_id(entity_name)
            assert computed == expected_object_id, (
                f"Entity '{entity_name}': Python computation mismatch. "
                f"Computed '{computed}', expected '{expected_object_id}'"
            )

            # Verify hash can be computed from the name
            hash_from_name = fnv1_hash_object_id(entity_name)
            assert hash_from_name == entity.key, (
                f"Entity '{entity_name}': hash mismatch. "
                f"Python hash {hash_from_name:#x}, API key {entity.key:#x}"
            )

        # === Test 2: Verify empty-name entity (has_own_name=false) ===
        # When entity has no name, the name field is empty in the API message
        # and the entity uses device's friendly_name (with MAC suffix) for display
        assert "" in entity_map, (
            "Empty-name entity not found. "
            f"Available entity names: {list(entity_map.keys())}"
        )
        empty_name_entity = entity_map[""]

        # object_id is computed from friendly_name (which includes MAC suffix)
        expected_object_id_empty = compute_expected_object_id(expected_friendly_name)
        assert empty_name_entity.object_id == expected_object_id_empty, (
            f"Empty-name entity: object_id mismatch. "
            f"API: '{empty_name_entity.object_id}', expected: '{expected_object_id_empty}'"
        )

        # Hash is also computed from friendly_name with MAC suffix
        expected_hash_empty = fnv1_hash_object_id(expected_friendly_name)
        assert empty_name_entity.key == expected_hash_empty, (
            f"Empty-name entity: hash mismatch. "
            f"API key: {empty_name_entity.key:#x}, expected: {expected_hash_empty:#x}"
        )

        # === Test 3: Verify ALL entities can have object_id computed from API data ===
        # This is the key property for removing object_id from the API protocol
        for entity in entities:
            # Use entity name if present, otherwise device's friendly_name
            name_for_object_id = entity.name or device_info.friendly_name

            # Compute object_id from the appropriate name
            computed_object_id = compute_expected_object_id(name_for_object_id)

            # Verify it matches what the API returned
            assert entity.object_id == computed_object_id, (
                f"Entity (name='{entity.name}'): object_id cannot be computed. "
                f"API: '{entity.object_id}', Computed from '{name_for_object_id}': '{computed_object_id}'"
            )

            # Verify hash can also be computed
            computed_hash = fnv1_hash_object_id(name_for_object_id)
            assert entity.key == computed_hash, (
                f"Entity (name='{entity.name}'): hash cannot be computed. "
                f"API key: {entity.key:#x}, Computed: {computed_hash:#x}"
            )
