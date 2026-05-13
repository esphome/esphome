"""Test get_base_entity_object_id function matches C++ behavior."""

from collections.abc import Callable, Generator
from pathlib import Path
import re
from typing import Any

import pytest

from esphome.config_validation import Invalid
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_DEVICE_ID,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_INTERNAL,
    CONF_NAME,
    CONF_UNIT_OF_MEASUREMENT,
)
from esphome.core import CORE, ID, entity_helpers
from esphome.core.entity_helpers import (
    _register_string,
    _setup_entity_impl,
    entity_duplicate_validator,
    finalize_entity_strings,
    get_base_entity_object_id,
    register_device_class,
    register_icon,
    setup_device_class,
    setup_entity,
    setup_unit_of_measurement,
)
from esphome.cpp_generator import MockObj
from esphome.helpers import sanitize, snake_case

from .common import load_config_from_fixture

# Pre-compiled regex pattern for extracting names from configure_entity_/set_name calls
# Matches: .configure_entity_("name", ...) or .set_name("name", ...)
ENTITY_NAME_PATTERN = re.compile(
    r'\.(?:configure_entity_|set_name)\(["\']([^"\']*)["\']'
)

FIXTURES_DIR = Path(__file__).parent.parent / "fixtures" / "core" / "entity_helpers"


@pytest.fixture(autouse=True)
def restore_core_state() -> Generator[None, None, None]:
    """Save and restore CORE state for tests."""
    original_name = CORE.name
    original_friendly_name = CORE.friendly_name
    yield
    CORE.name = original_name
    CORE.friendly_name = original_friendly_name


def test_with_entity_name() -> None:
    """Test when entity has its own name - should use entity name."""
    # Simple name
    assert get_base_entity_object_id("Temperature Sensor", None) == "temperature_sensor"
    assert (
        get_base_entity_object_id("Temperature Sensor", "Device Name")
        == "temperature_sensor"
    )
    # Even with device name, entity name takes precedence
    assert (
        get_base_entity_object_id("Temperature Sensor", "Device Name", "Sub Device")
        == "temperature_sensor"
    )

    # Name with special characters
    assert (
        get_base_entity_object_id("Temp!@#$%^&*()Sensor", None)
        == "temp__________sensor"
    )
    assert get_base_entity_object_id("Temp-Sensor_123", None) == "temp-sensor_123"

    # Already snake_case
    assert get_base_entity_object_id("temperature_sensor", None) == "temperature_sensor"

    # Mixed case
    assert get_base_entity_object_id("TemperatureSensor", None) == "temperaturesensor"
    assert get_base_entity_object_id("TEMPERATURE SENSOR", None) == "temperature_sensor"


def test_empty_name_with_device_name() -> None:
    """Test when entity has empty name and is on a sub-device - should use device name."""
    # C++ behavior: when has_own_name is false and device is set, uses device->get_name()
    assert (
        get_base_entity_object_id("", "Friendly Device", "Sub Device 1")
        == "sub_device_1"
    )
    assert (
        get_base_entity_object_id("", "Kitchen Controller", "controller_1")
        == "controller_1"
    )
    assert get_base_entity_object_id("", None, "Test-Device_123") == "test-device_123"


def test_empty_name_with_friendly_name() -> None:
    """Test when entity has empty name and no device - should use friendly name."""
    # C++ behavior: when has_own_name is false, uses App.get_friendly_name()
    assert get_base_entity_object_id("", "Friendly Device") == "friendly_device"
    assert get_base_entity_object_id("", "Kitchen Controller") == "kitchen_controller"
    assert get_base_entity_object_id("", "Test-Device_123") == "test-device_123"

    # Special characters in friendly name
    assert get_base_entity_object_id("", "Device!@#$%") == "device_____"


def test_empty_name_no_friendly_name() -> None:
    """Test when entity has empty name and no friendly name - should use device name."""
    # Test with CORE.name set
    CORE.name = "device-name"
    assert get_base_entity_object_id("", None) == "device-name"

    CORE.name = "Test Device"
    assert get_base_entity_object_id("", None) == "test_device"


def test_edge_cases() -> None:
    """Test edge cases."""
    # Only spaces
    assert get_base_entity_object_id("   ", None) == "___"

    # Unicode characters (should be replaced)
    assert get_base_entity_object_id("Température", None) == "temp_rature"
    assert get_base_entity_object_id("测试", None) == "__"

    # Empty string with empty friendly name (empty friendly name is treated as None)
    # Falls back to CORE.name
    CORE.name = "device"
    assert get_base_entity_object_id("", "") == "device"

    # Very long name (should work fine)
    long_name = "a" * 100 + " " + "b" * 100
    expected = "a" * 100 + "_" + "b" * 100
    assert get_base_entity_object_id(long_name, None) == expected


@pytest.mark.parametrize(
    ("name", "expected"),
    [
        ("Temperature Sensor", "temperature_sensor"),
        ("Living Room Light", "living_room_light"),
        ("Test-Device_123", "test-device_123"),
        ("Special!@#Chars", "special___chars"),
        ("UPPERCASE NAME", "uppercase_name"),
        ("lowercase name", "lowercase_name"),
        ("Mixed Case Name", "mixed_case_name"),
        ("   Spaces   ", "___spaces___"),
    ],
)
def test_matches_cpp_helpers(name: str, expected: str) -> None:
    """Test that the logic matches using snake_case and sanitize directly."""
    # For non-empty names, verify our function produces same result as direct snake_case + sanitize
    assert get_base_entity_object_id(name, None) == sanitize(snake_case(name))
    assert get_base_entity_object_id(name, None) == expected


def test_empty_name_fallback() -> None:
    """Test empty name handling which falls back to friendly_name or CORE.name."""
    # Empty name is handled specially - it doesn't just use sanitize(snake_case(""))
    # Instead it falls back to friendly_name or CORE.name
    assert sanitize(snake_case("")) == ""  # Direct conversion gives empty string
    # But our function returns a fallback
    CORE.name = "device"
    assert get_base_entity_object_id("", None) == "device"  # Uses device name


def test_name_add_mac_suffix_behavior() -> None:
    """Test behavior related to name_add_mac_suffix.

    In C++, when name_add_mac_suffix is enabled and entity has no name,
    get_object_id() returns str_sanitize(str_snake_case(App.get_friendly_name()))
    dynamically. Our function always returns the same result since we're
    calculating the base for duplicate tracking.
    """
    # The function should always return the same result regardless of
    # name_add_mac_suffix setting, as we're calculating the base object_id
    assert get_base_entity_object_id("", "Test Device") == "test_device"
    assert get_base_entity_object_id("Entity Name", "Test Device") == "entity_name"


def test_priority_order() -> None:
    """Test the priority order: entity name > device name > friendly name > CORE.name."""
    CORE.name = "core-device"

    # 1. Entity name has highest priority
    assert (
        get_base_entity_object_id("Entity Name", "Friendly Name", "Device Name")
        == "entity_name"
    )

    # 2. Device name is next priority (when entity name is empty)
    assert (
        get_base_entity_object_id("", "Friendly Name", "Device Name") == "device_name"
    )

    # 3. Friendly name is next (when entity and device names are empty)
    assert get_base_entity_object_id("", "Friendly Name", None) == "friendly_name"

    # 4. CORE.name is last resort
    assert get_base_entity_object_id("", None, None) == "core-device"


@pytest.mark.parametrize(
    ("name", "friendly_name", "device_name", "expected"),
    [
        # name, friendly_name, device_name, expected
        ("Living Room Light", None, None, "living_room_light"),
        ("", "Kitchen Controller", None, "kitchen_controller"),
        (
            "",
            "ESP32 Device",
            "controller_1",
            "controller_1",
        ),  # Device name takes precedence
        ("GPIO2 Button", None, None, "gpio2_button"),
        ("WiFi Signal", "My Device", None, "wifi_signal"),
        ("", None, "esp32_node", "esp32_node"),
        ("Front Door Sensor", "Home Assistant", "door_controller", "front_door_sensor"),
    ],
)
def test_real_world_examples(
    name: str, friendly_name: str | None, device_name: str | None, expected: str
) -> None:
    """Test real-world entity naming scenarios."""
    result = get_base_entity_object_id(name, friendly_name, device_name)
    assert result == expected


def test_issue_6953_scenarios() -> None:
    """Test specific scenarios from issue #6953."""
    # Scenario 1: Multiple empty names on main device with name_add_mac_suffix
    # The Python code calculates the base, C++ might append MAC suffix dynamically
    CORE.name = "device-name"
    CORE.friendly_name = "Friendly Device"

    # All empty names should resolve to same base
    assert get_base_entity_object_id("", CORE.friendly_name) == "friendly_device"
    assert get_base_entity_object_id("", CORE.friendly_name) == "friendly_device"
    assert get_base_entity_object_id("", CORE.friendly_name) == "friendly_device"

    # Scenario 2: Empty names on sub-devices
    assert (
        get_base_entity_object_id("", "Main Device", "controller_1") == "controller_1"
    )
    assert (
        get_base_entity_object_id("", "Main Device", "controller_2") == "controller_2"
    )

    # Scenario 3: xyz duplicates
    assert get_base_entity_object_id("xyz", None) == "xyz"
    assert get_base_entity_object_id("xyz", "Device") == "xyz"


# Tests for setup_entity function


@pytest.fixture
def setup_test_environment() -> Generator[list[str], None, None]:
    """Set up test environment for setup_entity tests."""
    # Set CORE state for tests
    CORE.name = "test-device"
    CORE.friendly_name = "Test Device"
    # Store original add function

    original_add = entity_helpers.add
    # Track what gets added
    added_expressions: list[str] = []

    def mock_add(expression: Any) -> Any:
        added_expressions.append(str(expression))
        return original_add(expression)

    # Patch add function in entity_helpers module
    entity_helpers.add = mock_add
    yield added_expressions
    # Clean up
    entity_helpers.add = original_add


def extract_object_id_from_config(config: dict[str, Any]) -> str | None:
    """Extract the object ID from config keys set by _setup_entity_impl."""
    name = config.get("_entity_name")
    if name is None:
        return None
    if name:
        return sanitize(snake_case(name))
    # Empty name - fall back to friendly_name or device name
    if CORE.friendly_name:
        return sanitize(snake_case(CORE.friendly_name))
    return sanitize(snake_case(CORE.name)) if CORE.name else None


def extract_object_id_from_expressions(expressions: list[str]) -> str | None:
    """Extract the object ID from configure_entity_() calls in generated expressions."""
    for expr in expressions:
        if match := ENTITY_NAME_PATTERN.search(expr):
            name = match.group(1)
            if name:
                return sanitize(snake_case(name))
            # Empty name - fall back to friendly_name or device name
            if CORE.friendly_name:
                return sanitize(snake_case(CORE.friendly_name))
            return sanitize(snake_case(CORE.name)) if CORE.name else None
    return None


@pytest.mark.asyncio
async def test_setup_entity_no_duplicates(setup_test_environment: list[str]) -> None:
    """Test setup_entity with unique names."""

    # Create mock entities
    var1 = MockObj("sensor1")
    var2 = MockObj("sensor2")

    # Set up first entity
    config1 = {
        CONF_NAME: "Temperature",
        CONF_DISABLED_BY_DEFAULT: False,
    }
    await _setup_entity_impl(var1, config1, "sensor")

    # Get object ID from first entity (stored in config, emitted later by finalize)
    object_id1 = extract_object_id_from_config(config1)
    assert object_id1 == "temperature"

    # Set up second entity with different name
    config2 = {
        CONF_NAME: "Humidity",
        CONF_DISABLED_BY_DEFAULT: False,
    }
    await _setup_entity_impl(var2, config2, "sensor")

    # Get object ID from second entity
    object_id2 = extract_object_id_from_config(config2)
    assert object_id2 == "humidity"


@pytest.mark.asyncio
async def test_setup_entity_different_platforms(
    setup_test_environment: list[str],
) -> None:
    """Test that same name on different platforms doesn't conflict."""

    # Create mock entities
    sensor = MockObj("sensor1")
    binary_sensor = MockObj("binary_sensor1")
    text_sensor = MockObj("text_sensor1")

    config = {
        CONF_NAME: "Status",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    # Set up entities on different platforms
    platforms = [
        (sensor, "sensor"),
        (binary_sensor, "binary_sensor"),
        (text_sensor, "text_sensor"),
    ]

    for var, platform in platforms:
        await _setup_entity_impl(var, config, platform)

    # All should get the same object ID (name stored in config, not platform-specific)
    assert extract_object_id_from_config(config) == "status"


@pytest.fixture
def mock_get_variable() -> Generator[dict[ID, MockObj], None, None]:
    """Mock get_variable to return test devices."""
    devices = {}
    original_get_variable = entity_helpers.get_variable

    async def _mock_get_variable(device_id: ID) -> MockObj:
        if device_id in devices:
            return devices[device_id]
        return await original_get_variable(device_id)

    entity_helpers.get_variable = _mock_get_variable
    yield devices
    # Clean up
    entity_helpers.get_variable = original_get_variable


@pytest.mark.asyncio
async def test_setup_entity_with_devices(
    setup_test_environment: list[str], mock_get_variable: dict[ID, MockObj]
) -> None:
    """Test that same name on different devices doesn't conflict."""

    # Create mock devices
    device1_id = ID("device1", type="Device")
    device2_id = ID("device2", type="Device")
    device1 = MockObj("device1_obj")
    device2 = MockObj("device2_obj")

    # Register devices with the mock
    mock_get_variable[device1_id] = device1
    mock_get_variable[device2_id] = device2

    # Create sensors with same name on different devices
    sensor1 = MockObj("sensor1")
    sensor2 = MockObj("sensor2")

    config1 = {
        CONF_NAME: "Temperature",
        CONF_DEVICE_ID: device1_id,
        CONF_DISABLED_BY_DEFAULT: False,
    }

    config2 = {
        CONF_NAME: "Temperature",
        CONF_DEVICE_ID: device2_id,
        CONF_DISABLED_BY_DEFAULT: False,
    }

    # Get object IDs
    for var, config in [(sensor1, config1), (sensor2, config2)]:
        await _setup_entity_impl(var, config, "sensor")

    # Both should get base object ID without suffix (different devices)
    assert extract_object_id_from_config(config1) == "temperature"
    assert extract_object_id_from_config(config2) == "temperature"


@pytest.mark.asyncio
async def test_setup_entity_empty_name(setup_test_environment: list[str]) -> None:
    """Test setup_entity with empty entity name."""

    var = MockObj("sensor1")

    config = {
        CONF_NAME: "",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    await _setup_entity_impl(var, config, "sensor")

    object_id = extract_object_id_from_config(config)
    # Should use friendly name
    assert object_id == "test_device"


@pytest.mark.asyncio
async def test_setup_entity_special_characters(
    setup_test_environment: list[str],
) -> None:
    """Test setup_entity with names containing special characters."""

    var = MockObj("sensor1")

    config = {
        CONF_NAME: "Temperature Sensor!",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    await _setup_entity_impl(var, config, "sensor")
    object_id = extract_object_id_from_config(config)

    # Special characters should be sanitized
    assert object_id == "temperature_sensor_"


@pytest.mark.asyncio
async def test_setup_entity_with_icon(setup_test_environment: list[str]) -> None:
    """Test setup_entity sets icon correctly."""

    var = MockObj("sensor1")

    config = {
        CONF_NAME: "Temperature",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "mdi:thermometer",
    }

    await _setup_entity_impl(var, config, "sensor")

    # Check icon index was stored in config for finalize_entity_strings
    assert config.get("_entity_icon_idx", 0) > 0


@pytest.mark.asyncio
async def test_setup_entity_disabled_by_default(
    setup_test_environment: list[str],
) -> None:
    """Test setup_entity sets disabled_by_default correctly."""

    var = MockObj("sensor1")

    config = {
        CONF_NAME: "Temperature",
        CONF_DISABLED_BY_DEFAULT: True,
    }

    await _setup_entity_impl(var, config, "sensor")

    # disabled_by_default is now packed into config for configure_entity_()
    assert config.get("_entity_disabled_by_default") == 1


def test_entity_duplicate_validator() -> None:
    """Test the entity_duplicate_validator function."""
    # Create validator for sensor platform
    validator = entity_duplicate_validator("sensor")

    # First entity should pass
    config1 = {CONF_NAME: "Temperature"}
    validated1 = validator(config1)
    assert validated1 == config1
    assert ("", "sensor", "temperature") in CORE.unique_ids
    # Check metadata was stored
    metadata = CORE.unique_ids[("", "sensor", "temperature")]
    assert metadata["name"] == "Temperature"
    assert metadata["platform"] == "sensor"

    # Second entity with different name should pass
    config2 = {CONF_NAME: "Humidity"}
    validated2 = validator(config2)
    assert validated2 == config2
    assert ("", "sensor", "humidity") in CORE.unique_ids
    metadata2 = CORE.unique_ids[("", "sensor", "humidity")]
    assert metadata2["name"] == "Humidity"

    # Duplicate entity should fail
    config3 = {CONF_NAME: "Temperature"}
    with pytest.raises(
        Invalid, match=r"Duplicate sensor entity with name 'Temperature' found"
    ):
        validator(config3)


def test_entity_duplicate_validator_with_devices() -> None:
    """Test entity_duplicate_validator with devices."""
    # Create validator for sensor platform
    validator = entity_duplicate_validator("sensor")

    # Create mock device IDs
    device1 = ID("device1", type="Device")
    device2 = ID("device2", type="Device")

    # Same name on different devices should pass
    config1 = {CONF_NAME: "Temperature", CONF_DEVICE_ID: device1}
    validated1 = validator(config1)
    assert validated1 == config1
    assert ("device1", "sensor", "temperature") in CORE.unique_ids
    metadata1 = CORE.unique_ids[("device1", "sensor", "temperature")]
    assert metadata1["device_id"] == "device1"

    config2 = {CONF_NAME: "Temperature", CONF_DEVICE_ID: device2}
    validated2 = validator(config2)
    assert validated2 == config2
    assert ("device2", "sensor", "temperature") in CORE.unique_ids
    metadata2 = CORE.unique_ids[("device2", "sensor", "temperature")]
    assert metadata2["device_id"] == "device2"

    # Duplicate on same device should fail
    config3 = {CONF_NAME: "Temperature", CONF_DEVICE_ID: device1}
    with pytest.raises(
        Invalid,
        match=r"Duplicate sensor entity with name 'Temperature' found on device 'device1'",
    ):
        validator(config3)


def test_duplicate_entity_yaml_validation(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that duplicate entity names are caught during YAML config validation."""
    result = load_config_from_fixture(yaml_file, "duplicate_entity.yaml", FIXTURES_DIR)
    assert result is None

    # Check for the duplicate entity error message
    captured = capsys.readouterr()
    assert "Duplicate sensor entity with name 'Temperature' found" in captured.out


def test_duplicate_entity_with_devices_yaml_validation(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test duplicate entity validation with devices."""
    result = load_config_from_fixture(
        yaml_file, "duplicate_entity_with_devices.yaml", FIXTURES_DIR
    )
    assert result is None

    # Check for the duplicate entity error message with device
    captured = capsys.readouterr()
    assert (
        "Duplicate sensor entity with name 'Temperature' found on device 'device1'"
        in captured.out
    )


def test_entity_different_platforms_yaml_validation(
    yaml_file: Callable[[str], str],
) -> None:
    """Test that same entity name on different platforms is allowed."""
    result = load_config_from_fixture(
        yaml_file, "entity_different_platforms.yaml", FIXTURES_DIR
    )
    # This should succeed
    assert result is not None


def test_entity_duplicate_validator_error_message() -> None:
    """Test that duplicate entity error messages include helpful metadata."""
    # Create validator for sensor platform
    validator = entity_duplicate_validator("sensor")

    # Set current component to simulate validation context for uptime sensor
    CORE.current_component = "sensor.uptime"

    # First entity should pass
    config1 = {CONF_NAME: "Battery", CONF_ID: ID("battery_1")}
    validated1 = validator(config1)
    assert validated1 == config1

    # Reset component to simulate template sensor
    CORE.current_component = "sensor.template"

    # Duplicate entity should fail with detailed error
    config2 = {CONF_NAME: "Battery", CONF_ID: ID("battery_2")}
    with pytest.raises(
        Invalid,
        match=r"Duplicate sensor entity with name 'Battery' found.*"
        r"Conflicts with entity 'Battery' \(id: battery_1\) from component 'sensor\.uptime'",
    ):
        validator(config2)

    # Clean up
    CORE.current_component = None


def test_entity_conflict_between_components_yaml(
    yaml_file: Callable[[str], str], capsys: pytest.CaptureFixture[str]
) -> None:
    """Test that conflicts between different components show helpful error messages."""
    result = load_config_from_fixture(
        yaml_file, "entity_conflict_components.yaml", FIXTURES_DIR
    )
    assert result is None

    # Check for the enhanced error message
    captured = capsys.readouterr()
    # The error should mention both the conflict and which component created it
    assert "Duplicate sensor entity with name 'Battery' found" in captured.out
    # Should mention it conflicts with an entity from a specific sensor platform
    assert "from component 'sensor." in captured.out
    # Should show it's a conflict between wifi_signal and template
    assert "sensor.wifi_signal" in captured.out or "sensor.template" in captured.out


def test_entity_duplicate_validator_internal_entities() -> None:
    """Test that internal entities are excluded from duplicate name validation."""
    # Create validator for sensor platform
    validator = entity_duplicate_validator("sensor")

    # First entity should pass
    config1 = {CONF_NAME: "Temperature"}
    validated1 = validator(config1)
    assert validated1 == config1
    # New format includes device_id (empty string for main device)
    assert ("", "sensor", "temperature") in CORE.unique_ids

    # Internal entity with same name should pass (not added to unique_ids)
    config2 = {CONF_NAME: "Temperature", CONF_INTERNAL: True}
    validated2 = validator(config2)
    assert validated2 == config2
    # Internal entity should not be added to unique_ids
    # Count how many times the key appears (should still be 1)
    count = sum(1 for k in CORE.unique_ids if k == ("", "sensor", "temperature"))
    assert count == 1

    # Another internal entity with same name should also pass
    config3 = {CONF_NAME: "Temperature", CONF_INTERNAL: True}
    validated3 = validator(config3)
    assert validated3 == config3
    # Still only one entry in unique_ids (from the non-internal entity)
    count = sum(1 for k in CORE.unique_ids if k == ("", "sensor", "temperature"))
    assert count == 1

    # Non-internal entity with same name should fail
    config4 = {CONF_NAME: "Temperature"}
    with pytest.raises(
        Invalid, match=r"Duplicate sensor entity with name 'Temperature' found"
    ):
        validator(config4)


def test_empty_or_null_device_id_on_entity() -> None:
    """Test that empty or null device IDs are handled correctly."""
    # Create validator for sensor platform
    validator = entity_duplicate_validator("sensor")

    # Entity with empty device_id should pass
    config1 = {CONF_NAME: "Battery", CONF_DEVICE_ID: ""}
    validated1 = validator(config1)
    assert validated1 == config1

    # Entity with None device_id should pass
    config2 = {CONF_NAME: "Temperature", CONF_DEVICE_ID: None}
    validated2 = validator(config2)
    assert validated2 == config2


def test_entity_duplicate_validator_non_ascii_names() -> None:
    """Test that non-ASCII names show helpful error messages."""
    # Create validator for binary_sensor platform
    validator = entity_duplicate_validator("binary_sensor")

    # First Russian sensor should pass
    config1 = {CONF_NAME: "Датчик открытия основного крана"}
    validated1 = validator(config1)
    assert validated1 == config1

    # Second Russian sensor with different text but same ASCII conversion should fail
    config2 = {CONF_NAME: "Датчик закрытия основного крана"}
    with pytest.raises(
        Invalid,
        match=re.compile(
            r"Duplicate binary_sensor entity with name 'Датчик закрытия основного крана' found.*"
            r"Original names: 'Датчик закрытия основного крана' and 'Датчик открытия основного крана'.*"
            r"Both convert to ASCII ID: '_______________________________'.*"
            r"To fix: Add unique ASCII characters \(e\.g\., '1', '2', or 'A', 'B'\)",
            re.DOTALL,
        ),
    ):
        validator(config2)


def test_entity_duplicate_validator_same_name_no_enhanced_message() -> None:
    """Test that identical names don't show the enhanced message."""
    # Create validator for sensor platform
    validator = entity_duplicate_validator("sensor")

    # First entity should pass
    config1 = {CONF_NAME: "Temperature"}
    validated1 = validator(config1)
    assert validated1 == config1

    # Second entity with exact same name should fail without enhanced message
    config2 = {CONF_NAME: "Temperature"}
    with pytest.raises(
        Invalid,
        match=r"Duplicate sensor entity with name 'Temperature' found.*"
        r"Each entity on a device must have a unique name within its platform\.$",
    ):
        validator(config2)


@pytest.mark.asyncio
async def test_setup_entity_empty_name_with_device(
    setup_test_environment: list[str],
) -> None:
    """Test setup_entity with empty entity name on a sub-device.

    For empty-name entities, Python passes 0 and C++ calculates the hash
    at runtime from the device's actual name.
    """
    added_expressions = setup_test_environment

    # Mock get_variable to return a mock device
    original_get_variable = entity_helpers.get_variable

    async def mock_get_variable(id_: ID) -> MockObj:
        return MockObj("sub_device_1")

    entity_helpers.get_variable = mock_get_variable

    var = MockObj("sensor1")
    device_id = ID("sub_device_1", type="Device")

    config = {
        CONF_NAME: "",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_DEVICE_ID: device_id,
    }

    await _setup_entity_impl(var, config, "sensor")

    entity_helpers.get_variable = original_get_variable

    # Check that set_device_ was called (separate protected call, accessible via friend)
    assert any("sensor1.set_device_" in expr for expr in added_expressions)

    # For empty-name entities, Python stores hash 0 - C++ calculates hash at runtime
    assert config.get("_entity_name") == ""
    assert config.get("_entity_object_id_hash") == 0


@pytest.mark.asyncio
async def test_setup_entity_empty_name_with_mac_suffix(
    setup_test_environment: list[str],
) -> None:
    """Test setup_entity with empty name and MAC suffix enabled.

    For empty-name entities, Python passes 0 and C++ calculates the hash
    at runtime from friendly_name (bug-for-bug compatibility).
    """

    # Set up CORE.config with name_add_mac_suffix enabled
    CORE.config = {"name_add_mac_suffix": True}
    # Set friendly_name to a specific value
    CORE.friendly_name = "My Device"

    var = MockObj("sensor1")

    config = {
        CONF_NAME: "",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    await _setup_entity_impl(var, config, "sensor")

    # For empty-name entities, Python stores hash 0 - C++ calculates hash at runtime
    assert config.get("_entity_name") == ""
    assert config.get("_entity_object_id_hash") == 0


@pytest.mark.asyncio
async def test_setup_entity_empty_name_with_mac_suffix_no_friendly_name(
    setup_test_environment: list[str],
) -> None:
    """Test setup_entity with empty name, MAC suffix enabled, but no friendly_name.

    For empty-name entities, Python passes 0 and C++ calculates the hash
    at runtime. In this case C++ will hash the empty friendly_name
    (bug-for-bug compatibility).
    """

    # Set up CORE.config with name_add_mac_suffix enabled
    CORE.config = {"name_add_mac_suffix": True}
    # Set friendly_name to empty
    CORE.friendly_name = ""

    var = MockObj("sensor1")

    config = {
        CONF_NAME: "",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    await _setup_entity_impl(var, config, "sensor")

    # For empty-name entities, Python stores hash 0 - C++ calculates hash at runtime
    assert config.get("_entity_name") == ""
    assert config.get("_entity_object_id_hash") == 0


@pytest.mark.asyncio
async def test_setup_entity_empty_name_no_mac_suffix_no_friendly_name(
    setup_test_environment: list[str],
) -> None:
    """Test setup_entity with empty name, no MAC suffix, and no friendly_name.

    For empty-name entities, Python passes 0 and C++ calculates the hash
    at runtime from the device name.
    """

    # No MAC suffix (either not set or False)
    CORE.config = {}
    # No friendly_name
    CORE.friendly_name = ""
    # Device name is set
    CORE.name = "my-test-device"

    var = MockObj("sensor1")

    config = {
        CONF_NAME: "",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    await _setup_entity_impl(var, config, "sensor")

    # For empty-name entities, Python stores hash 0 - C++ calculates hash at runtime
    assert config.get("_entity_name") == ""
    assert config.get("_entity_object_id_hash") == 0


def test_register_string_overflow() -> None:
    """Test _register_string raises ValueError when max count is exceeded."""
    category: dict[str, int] = {}
    for i in range(3):
        _register_string(f"val_{i}", category, 3, "test")
    with pytest.raises(ValueError, match="Too many unique test values"):
        _register_string("overflow", category, 3, "test")


def test_register_icon_max_length() -> None:
    """Test register_icon rejects icons exceeding 63 characters."""
    # 63 chars should succeed
    max_icon = "mdi:" + "a" * 59  # 63 total
    idx = register_icon(max_icon)
    assert idx > 0

    # 64 chars should fail
    too_long = "mdi:" + "a" * 60  # 64 total
    with pytest.raises(ValueError, match="Icon string too long"):
        register_icon(too_long)

    # Empty string returns 0
    assert register_icon("") == 0


def test_register_device_class_max_length() -> None:
    """Test register_device_class rejects device classes exceeding 47 characters."""
    # 47 chars should succeed
    max_dc = "a" * 47
    idx = register_device_class(max_dc)
    assert idx > 0

    # 48 chars should fail
    too_long = "a" * 48
    with pytest.raises(ValueError, match="Device class string too long"):
        register_device_class(too_long)

    # Empty string returns 0
    assert register_device_class("") == 0


@pytest.mark.asyncio
async def test_setup_entity_with_entity_category(
    setup_test_environment: list[str],
) -> None:
    """Test entity_category is packed correctly through the full setup flow."""
    added_expressions = setup_test_environment
    var = MockObj("sensor1")
    config = {
        CONF_NAME: "Temperature",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ENTITY_CATEGORY: "diagnostic",
    }
    await _setup_entity_impl(var, config, "sensor")
    finalize_entity_strings(var, config)
    packed = _extract_packed_value(added_expressions)
    assert packed != 0
    assert "category:diagnostic" in added_expressions[0]


@pytest.mark.asyncio
async def test_setup_entity_direct_call(setup_test_environment: list[str]) -> None:
    """Test setup_entity in direct call mode (legacy / backward compat)."""
    added_expressions = setup_test_environment

    var = MockObj("camera1")
    config = {
        CONF_NAME: "My Camera",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "mdi:camera",
    }

    # Direct call mode: await setup_entity(var, config, "camera")
    await setup_entity(var, config, "camera")

    # Should have emitted configure_entity_
    object_id = extract_object_id_from_expressions(added_expressions)
    assert object_id == "my_camera"

    # Icon index should have been stored and finalized
    assert config.get("_entity_icon_idx", 0) > 0


@pytest.mark.asyncio
async def test_setup_entity_decorator_mode(setup_test_environment: list[str]) -> None:
    """Test setup_entity in decorator mode."""
    added_expressions = setup_test_environment

    body_called = False

    @setup_entity("sensor")
    async def my_setup(var, config):
        nonlocal body_called
        body_called = True

    var = MockObj("sensor1")
    config = {
        CONF_NAME: "Temperature",
        CONF_DISABLED_BY_DEFAULT: False,
    }

    await my_setup(var, config)

    assert body_called
    object_id = extract_object_id_from_expressions(added_expressions)
    assert object_id == "temperature"


# Tests for finalize_entity_strings packing
#
# These tests verify that flags and string indices produce non-zero packed values
# and correct inline comments. The actual bit layout correctness (Python _*_SHIFT
# matching C++ ENTITY_FIELD_*_SHIFT) is verified end-to-end by the integration
# test test_host_mode_entity_fields, which compiles firmware and checks values
# via the native API.


def _extract_packed_value(expressions: list[str]) -> int:
    """Extract the third argument (packed value) from a configure_entity_() call."""
    for expr in expressions:
        if "configure_entity_" in expr:
            # Match the last integer argument before the closing ");"
            match = re.search(r",\s*(\d+)\s*\)", expr)
            if match:
                return int(match.group(1))
    raise AssertionError("No configure_entity_ call found")


@pytest.mark.asyncio
async def test_finalize_no_flags(setup_test_environment: list[str]) -> None:
    """Test entity with no special flags — packed value is 0, no comment."""
    added_expressions = setup_test_environment
    var = MockObj("sensor1")
    config = {
        CONF_NAME: "Test",
        CONF_DISABLED_BY_DEFAULT: False,
    }
    await _setup_entity_impl(var, config, "sensor")
    finalize_entity_strings(var, config)
    packed = _extract_packed_value(added_expressions)
    assert packed == 0
    assert "//" not in added_expressions[0]


@pytest.mark.asyncio
async def test_finalize_internal(setup_test_environment: list[str]) -> None:
    """Test entity with internal=True packs the internal flag."""
    added_expressions = setup_test_environment
    var = MockObj("sensor1")
    config = {
        CONF_NAME: "Test",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_INTERNAL: True,
    }
    await _setup_entity_impl(var, config, "sensor")
    finalize_entity_strings(var, config)
    packed = _extract_packed_value(added_expressions)
    assert packed != 0
    assert "// internal" in added_expressions[0]


@pytest.mark.asyncio
async def test_finalize_disabled_by_default(
    setup_test_environment: list[str],
) -> None:
    """Test entity with disabled_by_default=True packs the flag."""
    added_expressions = setup_test_environment
    var = MockObj("sensor1")
    config = {
        CONF_NAME: "Test",
        CONF_DISABLED_BY_DEFAULT: True,
    }
    await _setup_entity_impl(var, config, "sensor")
    finalize_entity_strings(var, config)
    packed = _extract_packed_value(added_expressions)
    assert packed != 0
    assert "// disabled_by_default" in added_expressions[0]


@pytest.mark.asyncio
async def test_finalize_entity_category(
    setup_test_environment: list[str],
) -> None:
    """Test entity_category values are packed and described in comment."""
    added_expressions = setup_test_environment
    var = MockObj("sensor1")

    # Test diagnostic
    config = {
        CONF_NAME: "Test",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ENTITY_CATEGORY: "diagnostic",
    }
    await _setup_entity_impl(var, config, "sensor")
    finalize_entity_strings(var, config)
    packed_diag = _extract_packed_value(added_expressions)
    assert packed_diag != 0
    assert "category:diagnostic" in added_expressions[0]

    # Test config — different packed value
    added_expressions.clear()
    config2 = {
        CONF_NAME: "Test2",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ENTITY_CATEGORY: "config",
    }
    await _setup_entity_impl(var, config2, "sensor")
    finalize_entity_strings(var, config2)
    packed_cfg = _extract_packed_value(added_expressions)
    assert packed_cfg != 0
    assert packed_cfg != packed_diag
    assert "category:config" in added_expressions[0]


@pytest.mark.asyncio
async def test_finalize_string_indices(
    setup_test_environment: list[str],
) -> None:
    """Test device_class, unit_of_measurement, and icon produce non-zero packed value."""
    added_expressions = setup_test_environment
    var = MockObj("sensor1")
    config = {
        CONF_NAME: "Test",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_DEVICE_CLASS: "temperature",
        CONF_UNIT_OF_MEASUREMENT: "°C",
        CONF_ICON: "mdi:thermometer",
    }
    await _setup_entity_impl(var, config, "sensor")
    setup_device_class(config)
    setup_unit_of_measurement(config)
    finalize_entity_strings(var, config)
    packed = _extract_packed_value(added_expressions)
    assert packed != 0
    comment = added_expressions[0]
    assert "dc:temperature" in comment
    assert "uom:°C" in comment
    assert "icon:mdi:thermometer" in comment


@pytest.mark.asyncio
async def test_finalize_all_fields(
    setup_test_environment: list[str],
) -> None:
    """Test all fields set: flags, string indices, and comment."""
    added_expressions = setup_test_environment
    var = MockObj("sensor1")
    config = {
        CONF_NAME: "Test",
        CONF_DISABLED_BY_DEFAULT: True,
        CONF_INTERNAL: True,
        CONF_ENTITY_CATEGORY: "diagnostic",
        CONF_DEVICE_CLASS: "temperature",
        CONF_UNIT_OF_MEASUREMENT: "°C",
        CONF_ICON: "mdi:thermometer",
    }
    await _setup_entity_impl(var, config, "sensor")
    setup_device_class(config)
    setup_unit_of_measurement(config)
    finalize_entity_strings(var, config)
    packed = _extract_packed_value(added_expressions)
    assert packed != 0
    # Verify comment contains all flags with actual string values
    comment_line = added_expressions[0]
    assert (
        "// internal, disabled_by_default, category:diagnostic,"
        " dc:temperature, uom:°C, icon:mdi:thermometer" in comment_line
    )


@pytest.mark.asyncio
async def test_finalize_comment_sanitization(
    setup_test_environment: list[str],
) -> None:
    """Test that user strings in comments are sanitized against injection."""
    added_expressions = setup_test_environment
    var = MockObj("sensor1")
    config = {
        CONF_NAME: "Test",
        CONF_DISABLED_BY_DEFAULT: False,
        # Backslash at end would cause line splice eating next code line
        CONF_ICON: "mdi:evil\\",
    }
    await _setup_entity_impl(var, config, "sensor")
    finalize_entity_strings(var, config)
    comment_line = added_expressions[0]
    # Backslash must be replaced to prevent line splice
    assert "\\" not in comment_line
    assert "mdi:evil/" in comment_line

    added_expressions.clear()
    config2 = {
        CONF_NAME: "Test2",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "mdi:evil\nINJECTED_CODE();",
    }
    await _setup_entity_impl(var, config2, "sensor")
    finalize_entity_strings(var, config2)
    comment_line = added_expressions[0]
    # Newline must be replaced to prevent breaking out of comment
    assert "\n" not in comment_line
    assert "INJECTED_CODE" in comment_line  # still visible but safe in comment
