"""Unit tests for esphome.components.packet_interface module."""

from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from esphome import config_validation as cv
from esphome.components import packet_interface
from esphome.const import CONF_ID


@pytest.fixture
def mock_packet_interface_class():
    """Create a mock PacketInterface subclass for testing."""
    mock_class = MagicMock()
    mock_class.__name__ = "MockPacketInterface"
    return mock_class


def test_packet_interface_schema_basic():
    """Test that packet_interface_schema creates valid schema with required ID."""
    mock_class = MagicMock()
    mock_class.__name__ = "TestClass"

    schema = packet_interface.packet_interface_schema(mock_class)

    # Verify schema is a cv.Schema instance
    assert isinstance(schema, cv.Schema)

    # Verify schema has ID field
    assert CONF_ID in schema.schema


def test_packet_interface_schema_validates_id_required():
    """Test that packet_interface_schema requires an ID field."""
    mock_class = MagicMock()
    mock_class.__name__ = "TestClass"

    schema = packet_interface.packet_interface_schema(mock_class)

    # Test that schema validates correctly with ID
    with patch("esphome.core.ID") as mock_id:
        mock_id_instance = MagicMock()
        mock_id.return_value = mock_id_instance

        config = {CONF_ID: "test_id"}
        # Schema should accept config with ID
        result = schema(config)
        assert CONF_ID in result


def test_packet_interface_schema_with_custom_class():
    """Test that packet_interface_schema accepts custom class types."""
    # Create a custom mock class
    custom_class = MagicMock()
    custom_class.__name__ = "CustomPacketInterface"

    schema = packet_interface.packet_interface_schema(custom_class)

    # Verify the schema was created
    assert isinstance(schema, cv.Schema)
    assert CONF_ID in schema.schema


@pytest.mark.asyncio
async def test_new_packet_interface_creates_instance(setup_core):
    """Test that new_packet_interface creates and registers PacketInterface instance correctly."""
    # Setup
    mock_id = MagicMock()
    mock_id.id = "test_packet_interface"
    config = {CONF_ID: mock_id}

    # Mock the code generation functions
    with (
        patch("esphome.codegen.new_Pvariable") as mock_new_pvariable,
        patch("esphome.codegen.register_component") as mock_register_component,
    ):
        # Setup mock return value
        mock_var = MagicMock()
        mock_new_pvariable.return_value = mock_var
        mock_register_component.return_value = AsyncMock()

        # Call the function
        result = await packet_interface.new_packet_interface(config)

        # Verify new_Pvariable was called with correct ID
        mock_new_pvariable.assert_called_once_with(mock_id)

        # Verify register_component was called
        mock_register_component.assert_called_once_with(mock_var, config)

        # Verify the result is the created variable
        assert result == mock_var


@pytest.mark.asyncio
async def test_new_packet_interface_with_additional_args(setup_core):
    """Test that new_packet_interface passes additional arguments correctly."""
    # Setup
    mock_id = MagicMock()
    mock_id.id = "test_packet_interface"
    config = {CONF_ID: mock_id}

    # Additional arguments to pass
    arg1 = "test_arg1"
    arg2 = 42
    arg3 = MagicMock()

    # Mock the code generation functions
    with (
        patch("esphome.codegen.new_Pvariable") as mock_new_pvariable,
        patch("esphome.codegen.register_component") as mock_register_component,
    ):
        # Setup mock return value
        mock_var = MagicMock()
        mock_new_pvariable.return_value = mock_var
        mock_register_component.return_value = AsyncMock()

        # Call the function with additional args
        result = await packet_interface.new_packet_interface(config, arg1, arg2, arg3)

        # Verify new_Pvariable was called with ID and additional args
        mock_new_pvariable.assert_called_once_with(mock_id, arg1, arg2, arg3)

        # Verify register_component was called
        mock_register_component.assert_called_once_with(mock_var, config)

        # Verify the result is the created variable
        assert result == mock_var


@pytest.mark.asyncio
async def test_new_packet_interface_registers_as_component(setup_core):
    """Test that new_packet_interface properly registers the instance as a Component."""
    # Setup
    mock_id = MagicMock()
    mock_id.id = "test_component"
    config = {
        CONF_ID: mock_id,
    }

    # Mock the code generation functions
    with (
        patch("esphome.codegen.new_Pvariable") as mock_new_pvariable,
        patch("esphome.codegen.register_component") as mock_register_component,
    ):
        # Setup mock return value
        mock_var = MagicMock()
        mock_new_pvariable.return_value = mock_var

        # Make register_component an async mock
        async def register_mock(var, cfg):
            # Simulate component registration
            pass

        mock_register_component.side_effect = register_mock

        # Call the function
        await packet_interface.new_packet_interface(config)

        # Verify the component was registered with the correct config
        mock_register_component.assert_called_once()
        call_args = mock_register_component.call_args
        assert call_args[0][0] == mock_var
        assert call_args[0][1] == config


def test_packet_interface_namespace_and_class_defined():
    """Test that the packet_interface namespace and PacketInterface class are properly defined."""
    # Verify namespace exists
    assert hasattr(packet_interface, "packet_interface_ns")
    assert packet_interface.packet_interface_ns is not None

    # Verify PacketInterface class exists
    assert hasattr(packet_interface, "PacketInterface")
    assert packet_interface.PacketInterface is not None


def test_packet_interface_is_platform_component():
    """Test that packet_interface is marked as a platform component."""
    assert packet_interface.IS_PLATFORM_COMPONENT is True


def test_packet_interface_has_codeowner():
    """Test that packet_interface has codeowners set."""
    assert hasattr(packet_interface, "CODEOWNERS")
    assert packet_interface.CODEOWNERS is not None
    assert len(packet_interface.CODEOWNERS) > 0
