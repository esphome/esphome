"""Common fixtures for dashboard tests."""

from __future__ import annotations

from unittest.mock import Mock

import pytest

from esphome.dashboard.core import ESPHomeDashboard


@pytest.fixture
def mock_dashboard() -> Mock:
    """Create a mock dashboard."""
    dashboard = Mock(spec=ESPHomeDashboard)
    dashboard.entries = Mock()
    dashboard.entries.async_all.return_value = []
    dashboard.stop_event = Mock()
    dashboard.stop_event.is_set.return_value = True
    dashboard.ping_request = Mock()
    return dashboard
