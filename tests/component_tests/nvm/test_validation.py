"""Tests for NVM component validation.

This module tests the configuration validation logic for the NVM component,
ensuring that invalid configurations are properly rejected with helpful error messages.
"""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.components import nvm
from esphome.core import CORE


class TestValidateNvmI2cAddress:
    """Tests for validate_nvm_i2c_address function."""

    def test_first_device_succeeds(self):
        """Test that the first NVM device with an I2C address is accepted."""
        config = {"id": "fram1", "address": 0x50}
        result = nvm.validate_nvm_i2c_address(config)
        assert result == config

    def test_duplicate_address_same_bus_fails(self):
        """Test that duplicate I2C address on same bus is rejected."""
        # First device should succeed
        config1 = {"id": "fram1", "address": 0x50}
        nvm.validate_nvm_i2c_address(config1)

        # Second device with same address should fail
        config2 = {"id": "fram2", "address": 0x50}
        with pytest.raises(cv.Invalid) as exc_info:
            nvm.validate_nvm_i2c_address(config2)

        error_msg = str(exc_info.value)
        assert "Duplicate NVM I2C address" in error_msg
        assert "0x50" in error_msg

    def test_different_addresses_same_bus_succeeds(self):
        """Test that different I2C addresses on same bus are allowed."""
        config1 = {"id": "fram1", "address": 0x50}
        nvm.validate_nvm_i2c_address(config1)

        config2 = {"id": "fram2", "address": 0x51}
        result = nvm.validate_nvm_i2c_address(config2)
        assert result == config2

    def test_same_address_different_buses_succeeds(self):
        """Test that same I2C address on different buses is allowed."""
        # Device on bus_a
        config1 = {"id": "fram1", "address": 0x50, "i2c_id": "bus_a"}
        nvm.validate_nvm_i2c_address(config1)

        # Device on bus_b with same address should succeed
        config2 = {"id": "fram2", "address": 0x50, "i2c_id": "bus_b"}
        result = nvm.validate_nvm_i2c_address(config2)
        assert result == config2

    def test_address_none_succeeds(self):
        """Test that config without address is accepted (non-I2C platform)."""
        config = {"id": "fram1"}
        result = nvm.validate_nvm_i2c_address(config)
        assert result == config


class TestValidatePreferencesPartitionCount:
    """Tests for validate_preferences_partition_count function."""

    def test_no_partitions_succeeds(self):
        """Test that config with no partitions is accepted."""
        config = {"id": "fram1", "partitions": []}
        result = nvm.validate_preferences_partition_count(config)
        assert result == config

    def test_single_preferences_partition_succeeds(self):
        """Test that a single preferences partition is accepted."""
        config = {
            "id": "fram1",
            "partitions": [
                {"id": "prefs", "type": "preferences", "size": 4096},
            ],
        }
        result = nvm.validate_preferences_partition_count(config)
        assert result == config

    def test_multiple_preferences_partitions_fails(self):
        """Test that multiple preferences partitions are rejected."""
        # First config with preferences partition
        config1 = {
            "id": "fram1",
            "partitions": [
                {"id": "prefs1", "type": "preferences", "size": 4096},
            ],
        }
        nvm.validate_preferences_partition_count(config1)

        # Second config with preferences partition should fail
        config2 = {
            "id": "fram2",
            "partitions": [
                {"id": "prefs2", "type": "preferences", "size": 4096},
            ],
        }
        with pytest.raises(cv.Invalid) as exc_info:
            nvm.validate_preferences_partition_count(config2)

        error_msg = str(exc_info.value)
        assert "Only one preferences partition" in error_msg

    def test_mixed_partition_types_succeeds(self):
        """Test that mixed partition types (only one preferences) are accepted."""
        config = {
            "id": "fram1",
            "partitions": [
                {"id": "prefs", "type": "preferences", "size": 4096},
                {"id": "raw", "type": "raw", "size": 8192},
                {"id": "kv", "type": "key_value", "size": 2048},
            ],
        }
        result = nvm.validate_preferences_partition_count(config)
        assert result == config

    def test_raw_partitions_only_succeeds(self):
        """Test that multiple raw partitions are accepted."""
        config = {
            "id": "fram1",
            "partitions": [
                {"id": "raw1", "type": "raw", "size": 4096},
                {"id": "raw2", "type": "raw", "size": 4096},
            ],
        }
        result = nvm.validate_preferences_partition_count(config)
        assert result == config


class TestNvmData:
    """Tests for NvmData state management."""

    def test_data_stored_in_core_data(self):
        """Test that NVM data is stored in CORE.data."""
        data = nvm._get_data()
        assert "nvm" in CORE.data
        assert CORE.data["nvm"] is data

    def test_data_is_reset_with_core(self):
        """Test that NVM data is reset when CORE is reset."""
        # Create some data
        nvm._get_data().preferences_partition_count = 5
        nvm._get_data().i2c_devices["test"] = "value"

        # Reset CORE
        CORE.reset()

        # Data should be reset
        data = nvm._get_data()
        assert data.preferences_partition_count == 0
        assert data.i2c_devices == {}
