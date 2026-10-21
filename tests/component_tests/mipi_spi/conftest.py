"""Tests for mpip_spi configuration validation."""

from unittest import mock

import pytest

# choose_variant_with_pins is provided by the shared parent conftest.


@pytest.fixture(autouse=True)
def mock_spi_final_validate():
    """Mock spi.final_validate_device_schema since unit tests have no real SPI bus config."""
    with mock.patch(
        "esphome.components.spi.final_validate_device_schema",
        return_value=lambda config: None,
    ):
        yield
