"""Tests for esphome.dashboard.status.mqtt."""

from __future__ import annotations

import threading
from unittest.mock import Mock, patch

from esphome.core import EsphomeError
from esphome.dashboard.status.mqtt import MqttStatusThread


def test_mqtt_status_thread_retries_connect_and_recovers() -> None:
    """Test that transient connect failures do not permanently kill the thread."""
    stop_event = threading.Event()

    mqtt_ping_request = Mock()
    mqtt_ping_request.wait.return_value = True
    mqtt_ping_request.clear = Mock()

    entries = Mock()
    entries.all.return_value = []
    entries.get_by_name.return_value = []
    entries.set_state_if_online_or_source = Mock()
    entries.set_state_if_source = Mock()

    dashboard = Mock()
    dashboard.entries = entries
    dashboard.stop_event = stop_event
    dashboard.mqtt_ping_request = mqtt_ping_request

    loop_started = threading.Event()
    client = Mock()
    client.loop_start.side_effect = loop_started.set

    attempt = {"count": 0}

    def prepare_side_effect(*args, **kwargs):
        attempt["count"] += 1
        if attempt["count"] < 3:
            raise EsphomeError("Cannot connect to MQTT broker: temporary failure")
        return client

    with (
        patch(
            "esphome.dashboard.status.mqtt.mqtt.prepare",
            side_effect=prepare_side_effect,
        ) as mock_prepare,
        patch("esphome.dashboard.status.mqtt._CONNECT_RETRY_INITIAL_DELAY", 0.01),
        patch("esphome.dashboard.status.mqtt._CONNECT_RETRY_MAX_DELAY", 0.02),
        patch("esphome.dashboard.status.mqtt._POLL_INTERVAL", 0.01),
    ):
        thread = MqttStatusThread(dashboard)
        thread.start()

        assert loop_started.wait(1.0)
        stop_event.set()

        thread.join(timeout=1.0)
        assert not thread.is_alive()

        assert mock_prepare.call_count >= 3
        client.disconnect.assert_called_once()
        client.loop_stop.assert_called_once()


def test_mqtt_status_thread_returns_if_stopped_before_connect() -> None:
    """If the dashboard is already stopping, do not attempt an MQTT connection."""
    stop_event = threading.Event()
    stop_event.set()

    entries = Mock()
    entries.all.return_value = []

    dashboard = Mock()
    dashboard.entries = entries
    dashboard.stop_event = stop_event
    dashboard.mqtt_ping_request = Mock()

    with patch("esphome.dashboard.status.mqtt.mqtt.prepare") as mock_prepare:
        # Call run() directly to avoid thread scheduling flakiness.
        MqttStatusThread(dashboard).run()

    mock_prepare.assert_not_called()


def test_mqtt_status_thread_stops_during_retry_wait() -> None:
    """If stop_event is set while waiting between retries, the thread should exit."""
    stop_event = Mock()
    stop_event.is_set.return_value = False
    # Simulate the stop event being set during the retry wait.
    stop_event.wait.return_value = True

    mqtt_ping_request = Mock()
    mqtt_ping_request.wait.return_value = True
    mqtt_ping_request.clear = Mock()

    entries = Mock()
    entries.all.return_value = []
    entries.get_by_name.return_value = []
    entries.set_state_if_online_or_source = Mock()
    entries.set_state_if_source = Mock()

    dashboard = Mock()
    dashboard.entries = entries
    dashboard.stop_event = stop_event
    dashboard.mqtt_ping_request = mqtt_ping_request

    with (
        patch(
            "esphome.dashboard.status.mqtt.mqtt.prepare",
            side_effect=EsphomeError("Cannot connect to MQTT broker: temporary failure"),
        ) as mock_prepare,
        patch("esphome.dashboard.status.mqtt._CONNECT_RETRY_INITIAL_DELAY", 0.01),
        patch("esphome.dashboard.status.mqtt._CONNECT_RETRY_MAX_DELAY", 0.02),
    ):
        # Call run() directly to deterministically hit the retry branch.
        MqttStatusThread(dashboard).run()

    # Exactly one attempt: we exit during the first wait().
    assert mock_prepare.call_count == 1
