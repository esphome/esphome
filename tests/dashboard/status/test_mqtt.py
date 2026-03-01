"""Tests for esphome.dashboard.status.mqtt."""

from __future__ import annotations

import threading
from unittest.mock import Mock, patch

from esphome.core import EsphomeError
from esphome.dashboard.entries import EntryStateSource, bool_to_entry_state
from esphome.dashboard.status.mqtt import MqttStatusThread


def test_mqtt_status_thread_retries_connect_and_recovers() -> None:
    """Test that transient connect failures do not permanently kill the thread."""
    stop_event = threading.Event()

    mqtt_ping_request = Mock()
    mqtt_ping_request.wait.return_value = True
    mqtt_ping_request.clear = Mock()

    entries = Mock()
    # Ensure at least one poll-loop iteration does real work
    entries.all.return_value = [Mock()]
    entries.get_by_name.return_value = []
    entries.set_state_if_online_or_source = Mock()
    poll_iteration = threading.Event()
    entries.set_state_if_source = Mock(
        side_effect=lambda *args, **kwargs: poll_iteration.set()
    )

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
        # Ensure we enter the _POLL_INTERVAL loop at least once (Codecov partial branch).
        assert poll_iteration.wait(1.0)
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
            side_effect=EsphomeError(
                "Cannot connect to MQTT broker: temporary failure"
            ),
        ) as mock_prepare,
        patch("esphome.dashboard.status.mqtt._CONNECT_RETRY_INITIAL_DELAY", 0.01),
        patch("esphome.dashboard.status.mqtt._CONNECT_RETRY_MAX_DELAY", 0.02),
    ):
        # Call run() directly to deterministically hit the retry branch.
        MqttStatusThread(dashboard).run()

    # Exactly one attempt: we exit during the first wait().
    assert mock_prepare.call_count == 1


def test_mqtt_status_thread_tracks_host_status_topic_and_prevents_offline_reset() -> (
    None
):
    """Host entries with `<name>/status=online` should not be forced offline by polling."""
    stop_event = threading.Event()

    mqtt_ping_request = Mock()
    mqtt_ping_request.clear = Mock()

    entries = Mock()
    host_entry = Mock()
    host_entry.name = "host1"
    host_entry.target_platform = "host"
    entries.all.return_value = [host_entry]
    entries.get_by_name.side_effect = lambda name: (
        {host_entry} if name == "host1" else []
    )
    entries.set_state_if_online_or_source = Mock()
    entries.set_state_if_source = Mock()

    dashboard = Mock()
    dashboard.entries = entries
    dashboard.stop_event = stop_event
    dashboard.mqtt_ping_request = mqtt_ping_request

    client = Mock()

    def prepare_side_effect(config, topics, on_message, on_connect, *args, **kwargs):
        assert "esphome/discover/#" in topics
        assert "+/status" in topics
        msg = Mock()
        msg.topic = "host1/status"
        msg.payload = b"online"
        on_message(client, None, msg)
        return client

    def wait_side_effect(*args, **kwargs):
        stop_event.set()
        return True

    mqtt_ping_request.wait.side_effect = wait_side_effect

    with (
        patch(
            "esphome.dashboard.status.mqtt.mqtt.prepare",
            side_effect=prepare_side_effect,
        ),
        patch("esphome.dashboard.status.mqtt._POLL_INTERVAL", 0.01),
    ):
        MqttStatusThread(dashboard).run()

    entries.set_state_if_online_or_source.assert_called_with(
        host_entry, bool_to_entry_state(True, EntryStateSource.MQTT)
    )
    entries.set_state_if_source.assert_not_called()


def test_mqtt_status_thread_ignores_status_topic_for_non_host_entries() -> None:
    """Non-host entries should not be marked online based on `<name>/status`."""
    stop_event = threading.Event()

    mqtt_ping_request = Mock()
    mqtt_ping_request.clear = Mock()

    entries = Mock()
    entry = Mock()
    entry.name = "node1"
    entry.target_platform = "esp32"
    entries.all.return_value = [entry]
    entries.get_by_name.side_effect = lambda name: {entry} if name == "node1" else []
    entries.set_state_if_online_or_source = Mock()
    entries.set_state_if_source = Mock()

    dashboard = Mock()
    dashboard.entries = entries
    dashboard.stop_event = stop_event
    dashboard.mqtt_ping_request = mqtt_ping_request

    client = Mock()

    def prepare_side_effect(config, topics, on_message, on_connect, *args, **kwargs):
        msg = Mock()
        msg.topic = "node1/status"
        msg.payload = b"online"
        on_message(client, None, msg)
        return client

    def wait_side_effect(*args, **kwargs):
        stop_event.set()
        return True

    mqtt_ping_request.wait.side_effect = wait_side_effect

    with (
        patch(
            "esphome.dashboard.status.mqtt.mqtt.prepare",
            side_effect=prepare_side_effect,
        ),
        patch("esphome.dashboard.status.mqtt._POLL_INTERVAL", 0.01),
    ):
        MqttStatusThread(dashboard).run()

    entries.set_state_if_online_or_source.assert_not_called()
    entries.set_state_if_source.assert_called()
