"""Tests to verify preference and entity key hash values remain stable.

These tests ensure the hash algorithms do NOT change, as any change would cause
users to lose stored preferences (calibration values, restore states, etc.) on
firmware upgrades, or break entity state routing to API clients.

Two algorithms are locked here (see https://github.com/esphome/backlog/issues/85):
1. `fnv1_hash_object_id(name)` - the object_id hash (snake_case + sanitize, then FNV-1).
   The entity key sent to API clients and the base of every stored preference key.
2. `fnv1_hash_name(name)` - FNV-1 over the raw UTF-8 name bytes. 2026.8 beta
   firmware stored preferences under keys derived from it; a future key migration
   must reconstruct those keys to recover that data.

DO NOT CHANGE THE EXPECTED VALUES - if tests fail after modifying a hash algorithm,
the change breaks backward compatibility and will cause data loss.
"""

import pytest

from esphome.helpers import (
    FNV1_OFFSET_BASIS,
    FNV1_PRIME,
    fnv1_hash_name,
    fnv1_hash_object_id,
)

# =============================================================================
# Test: fnv1_hash_object_id produces stable hashes for entity names
# =============================================================================


@pytest.mark.parametrize(
    ("entity_name", "expected_object_id_hash"),
    [
        # =====================================================================
        # Core entity types - these names appear in many ESPHome configurations
        # =====================================================================
        # Basic single-word names
        ("Light", 0x735CF023),
        ("Switch", 0xBEDF78E5),
        ("Sensor", 0x75E61B1B),
        ("Fan", 0x468F6780),
        ("Climate", 0xAA22FD4A),
        ("Cover", 0xA630D0A2),
        ("Lock", 0x1D2FD708),
        ("Valve", 0x25ED5F65),
        ("Button", 0x3A42C455),
        ("Number", 0xB900E22A),
        ("Select", 0x556391B5),
        ("Text", 0xB12BFA38),
        # Multi-word names (spaces become underscores, lowercase)
        ("Living Room Light", 0xC6F81EC9),
        ("Kitchen Switch", 0xC63C0F6E),
        ("Temperature Sensor", 0x16AF55B6),
        ("Garage Door Cover", 0x685E5281),
        ("Bedroom Fan", 0x21AB1DED),
        ("Front Door Lock", 0xB9BEF8E1),
        # Already snake_case names (should hash same as space-separated)
        ("living_room_light", 0xC6F81EC9),  # Same as "Living Room Light"
        ("kitchen_switch", 0xC63C0F6E),  # Same as "Kitchen Switch"
        # Names with numbers
        ("Sensor 1", 0x99828E4B),
        ("Relay 2", 0x6FFEF2FB),
        ("Zone 10", 0xFD83AA95),
        # Names with special characters (become underscores)
        ("AC Unit", 0x336C6886),
        ("WiFi Signal", 0x2FA52175),
        ("CO2 Level", 0x31049870),
        # Mixed case handling
        ("mySwitch", 0x9AA10553),
        ("MySwitch", 0x9AA10553),  # Same as lowercase
        ("MYSWITCH", 0x9AA10553),  # Same as lowercase
        # =====================================================================
        # Edge cases
        # =====================================================================
        # Empty name (hashes to the FNV-1 offset basis since no chars processed)
        ("", 0x811C9DC5),
        # Single character
        ("a", 0x050C5D7E),
        ("A", 0x050C5D7E),  # Same after lowercase
        ("1", 0x050C5D2E),
        ("_", 0x050C5D40),
        # Names that differ only in case (should hash identically)
        ("test", 0xBC2C0BE9),
        ("Test", 0xBC2C0BE9),
        ("TEST", 0xBC2C0BE9),
        # Names that differ only in spaces vs underscores (should hash identically)
        ("foo bar", 0x3AE35AA1),
        ("foo_bar", 0x3AE35AA1),
        ("Foo Bar", 0x3AE35AA1),
        ("FOO_BAR", 0x3AE35AA1),
        # Non-ASCII names (sanitized per code point, one underscore per character)
        ("äöü", 0x10028B12),
        ("温度", 0x3276CB9F),
        ("Température", 0x965698F3),
        # =====================================================================
        # Real-world component entity names from ESPHome codebase
        # =====================================================================
        # From fan.cpp - FanRestoreState
        ("Ceiling Fan", 0x640DEF00),
        # From climate.cpp - ClimateRestoreState
        ("HVAC", 0xDD68438B),
        ("Thermostat", 0x30A5B7C6),
        # From light/light_state.cpp
        ("LED Strip", 0x2A068423),
        ("Dimmable Light", 0xD70393F3),
        # From cover/cover.cpp
        ("Garage Door", 0x53987A5D),
        ("Window Blind", 0x851291A5),
        # From switch/switch.cpp
        ("Relay", 0xD3A92FE4),
        ("Power Switch", 0x5C4A47B3),
        # From number/automation.cpp
        ("Brightness", 0xF46E252C),
        ("Volume", 0x8FFEBE43),
        # From template datetime entities
        ("Wake Time", 0xEE612B53),
        ("Schedule Date", 0xF538C8DD),
    ],
)
def test_entity_object_id_hash_stability(
    entity_name: str, expected_object_id_hash: int
) -> None:
    """Verify fnv1_hash_object_id produces stable hashes for entity names.

    CRITICAL: These expected values MUST NOT CHANGE. Existing devices have
    preferences stored under keys derived from this hash, and it is the entity
    key sent to API clients; changing it loses stored preferences and breaks
    entity state routing.
    """
    actual = fnv1_hash_object_id(entity_name)
    assert actual == expected_object_id_hash, (
        f"Hash for '{entity_name}' changed from {expected_object_id_hash:#010x} to {actual:#010x}. "
        f"This will cause users to lose stored preferences!"
    )


# =============================================================================
# Test: Legacy preference key computation formula
# =============================================================================


def compute_legacy_preference_key(
    entity_name: str, version: int = 0, device_id: int = 0
) -> int:
    """Compute the legacy preference key: (object_id_hash ^ device_id) ^ version.

    This is the key EntityBase::make_entity_preference_() (entity_base.cpp)
    stores every entity preference under.
    """
    object_id_hash = fnv1_hash_object_id(entity_name)
    preference_hash = object_id_hash ^ device_id
    key = preference_hash ^ version
    return key & 0xFFFFFFFF


# Restore state version constants from ESPHome components
# These MUST match the RESTORE_STATE_VERSION values in the C++ code
FAN_RESTORE_STATE_VERSION = 0x71700ABA  # From fan/fan.cpp
CLIMATE_RESTORE_STATE_VERSION = 0x848EA6AD  # From climate/climate.cpp


@pytest.mark.parametrize(
    ("entity_name", "version", "device_id", "expected_key"),
    [
        # No version, main device (key equals the plain object_id hash)
        ("Test Sensor", 0, 0, 0x5D74FA46),
        ("Light", 0, 0, 0x735CF023),
        # Restore state versions on the main device
        ("Ceiling Fan", FAN_RESTORE_STATE_VERSION, 0, 0x157DE5BA),
        ("HVAC", CLIMATE_RESTORE_STATE_VERSION, 0, 0x59E6E526),
        # Sub-devices: same entity name on different devices gets different keys
        ("Light", 0, 1, 0x735CF022),
        ("Fan", FAN_RESTORE_STATE_VERSION, 0xABCD, 0x37FFC6F7),
    ],
)
def test_legacy_preference_key_computation(
    entity_name: str, version: int, device_id: int, expected_key: int
) -> None:
    """Verify legacy preference key computation matches expected values.

    This test ensures the formula doesn't change, which would lose stored
    preferences on every platform.
    """
    actual_key = compute_legacy_preference_key(entity_name, version, device_id)

    assert actual_key == expected_key, (
        f"Preference key for '{entity_name}' (version={version:#x}, device_id={device_id}) "
        f"changed from {expected_key:#010x} to {actual_key:#010x}. "
        f"This will cause users to lose stored preferences!"
    )


# =============================================================================
# Test: fnv1_hash_name produces stable entity keys (raw name, UTF-8 bytes)
# =============================================================================


@pytest.mark.parametrize(
    ("entity_name", "expected_key"),
    [
        # ASCII names
        ("Temperature Sensor", 0x801C3665),
        ("LED Strip", 0xD5C7B082),
        ("Garage Door", 0x2D70E086),
        ("Relay", 0x565177C4),
        # Raw names are case and space sensitive, unlike the old object_id hash
        ("temperature sensor", 0xF9F431E5),
        # Non-ASCII names hash their UTF-8 bytes and stay distinct
        ("Датчик открытия", 0x001861C1),
        ("温度", 0x8EDF61C9),
        ("Température", 0x531A74AA),
        # Empty name hashes to the FNV-1 offset basis
        ("", 0x811C9DC5),
    ],
)
def test_entity_key_hash_stability(entity_name: str, expected_key: int) -> None:
    """Verify fnv1_hash_name produces stable raw-name hashes.

    CRITICAL: These expected values MUST NOT CHANGE. 2026.8 beta firmware stored
    preferences under keys derived from this hash; a future key migration must
    reconstruct those keys, and changing the algorithm would strand that data.
    Matched C++ fnv1_hash_bytes() (2026.8 beta), which the unrevert restores.
    """
    actual = fnv1_hash_name(entity_name)
    assert actual == expected_key, (
        f"Entity key for '{entity_name}' changed from {expected_key:#010x} to {actual:#010x}. "
        f"This breaks state routing and stored preferences!"
    )


def test_fnv1_hash_name_matches_utf8_byte_hash() -> None:
    """Verify fnv1_hash_name hashes the UTF-8 encoded bytes of the name."""
    name = "Température 温度"
    hash_value = FNV1_OFFSET_BASIS
    for byte in name.encode("utf-8"):
        hash_value = (hash_value * FNV1_PRIME) & 0xFFFFFFFF
        hash_value ^= byte
    assert fnv1_hash_name(name) == hash_value
