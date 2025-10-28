"""Common fixtures for dashboard tests."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import MagicMock, Mock

import pytest
import pytest_asyncio

from esphome.dashboard.core import ESPHomeDashboard
from esphome.dashboard.entries import DashboardEntries


@pytest.fixture
def mock_settings(tmp_path: Path) -> MagicMock:
    """Create mock dashboard settings."""
    settings = MagicMock()
    settings.config_dir = str(tmp_path)
    settings.absolute_config_dir = tmp_path
    return settings


@pytest.fixture
def mock_dashboard(mock_settings: MagicMock) -> Mock:
    """Create a mock dashboard."""
    dashboard = Mock(spec=ESPHomeDashboard)
    dashboard.settings = mock_settings
    dashboard.entries = Mock()
    dashboard.entries.async_all.return_value = []
    dashboard.stop_event = Mock()
    dashboard.stop_event.is_set.return_value = True
    dashboard.ping_request = Mock()
    dashboard.ignored_devices = set()
    dashboard.bus = Mock()
    dashboard.bus.async_fire = Mock()
    return dashboard


@pytest_asyncio.fixture
async def dashboard_entries(mock_dashboard: Mock) -> DashboardEntries:
    """Create a DashboardEntries instance for testing."""
    return DashboardEntries(mock_dashboard)
