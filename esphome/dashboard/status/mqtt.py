from __future__ import annotations

import binascii
import json
import logging
import os
import threading
import typing

from esphome import mqtt
from esphome.core import EsphomeError

from ..entries import EntryStateSource, bool_to_entry_state

if typing.TYPE_CHECKING:
    from ..core import ESPHomeDashboard


_LOGGER = logging.getLogger(__name__)

# How often to re-check entries and publish a discover ping.
_POLL_INTERVAL = 2.0

# Retry behavior for initial broker connect failures (e.g. transient DNS/network readiness).
_CONNECT_RETRY_INITIAL_DELAY = 1.0
_CONNECT_RETRY_MAX_DELAY = 300.0


class MqttStatusThread(threading.Thread):
    """Status thread to get the status of the devices via MQTT."""

    def __init__(self, dashboard: ESPHomeDashboard) -> None:
        """Initialize the status thread."""
        super().__init__()
        self.dashboard = dashboard

    @staticmethod
    def _extract_name_from_default_status_topic(topic: str) -> str | None:
        """Extract an entry name from a default MQTT status topic.

        ESPHome's default MQTT birth/will topic is `<topic_prefix>/status`, and by default
        `topic_prefix` is the node name. For safety we only treat topics of the exact form
        `<name>/status` (one segment) as candidates.
        """
        if not topic.endswith("/status"):
            return None
        if topic.count("/") != 1:
            return None
        name = topic.split("/", 1)[0]
        return name or None

    def run(self) -> None:
        """Run the status thread."""
        dashboard = self.dashboard
        entries = dashboard.entries
        current_entries = entries.all()

        config = mqtt.config_from_env()
        discover_topic = "esphome/discover/#"
        status_topic = "+/status"
        online_from_status: set[str] = set()

        def on_message(client, userdata, msg):
            payload = msg.payload.decode(errors="backslashreplace")
            if (
                status_name := self._extract_name_from_default_status_topic(msg.topic)
            ) is not None:
                if not (matching_entries := entries.get_by_name(status_name)):
                    return

                # Tight heuristic: only treat `<name>/status` as authoritative for host nodes.
                host_entries = [
                    entry
                    for entry in matching_entries
                    if entry.target_platform == "host"
                ]
                if not host_entries:
                    return

                if payload == "online":
                    online_from_status.add(status_name)
                    for entry in host_entries:
                        entries.set_state_if_online_or_source(
                            entry, bool_to_entry_state(True, EntryStateSource.MQTT)
                        )
                elif payload == "offline":
                    online_from_status.discard(status_name)
                    for entry in host_entries:
                        entries.set_state_if_source(
                            entry, bool_to_entry_state(False, EntryStateSource.MQTT)
                        )
                return

            if not payload or not msg.topic.startswith("esphome/discover/"):
                return

            try:
                data = json.loads(payload)
            except json.JSONDecodeError:
                return
            if "name" not in data:
                return
            if matching_entries := entries.get_by_name(data["name"]):
                for entry in matching_entries:
                    # Only override state if we don't have a state from another source
                    # or we have a state from MQTT and the device is reachable
                    entries.set_state_if_online_or_source(
                        entry, bool_to_entry_state(True, EntryStateSource.MQTT)
                    )

        def on_connect(client, userdata, flags, return_code):
            client.publish("esphome/discover", None, retain=False)

        mqttid = str(binascii.hexlify(os.urandom(6)).decode())

        client = None
        retry_delay = _CONNECT_RETRY_INITIAL_DELAY
        while client is None and not dashboard.stop_event.is_set():
            try:
                client = mqtt.prepare(
                    config,
                    [discover_topic, status_topic],
                    on_message,
                    on_connect,
                    None,
                    None,
                    f"esphome-dashboard-{mqttid}",
                )
            except EsphomeError as err:
                _LOGGER.warning(
                    "Cannot connect to MQTT broker for dashboard status: %s. Retrying in %.1f s",
                    err,
                    retry_delay,
                )
                if dashboard.stop_event.wait(retry_delay):
                    return
                retry_delay = min(retry_delay * 2, _CONNECT_RETRY_MAX_DELAY)

        if client is None:
            return
        client.loop_start()

        while not dashboard.stop_event.wait(_POLL_INTERVAL):
            current_entries = entries.all()
            # will be set to true on on_message
            for entry in current_entries:
                # Only override state if we don't have a state from another source
                if entry.target_platform == "host" and entry.name in online_from_status:
                    continue
                entries.set_state_if_source(
                    entry, bool_to_entry_state(False, EntryStateSource.MQTT)
                )

            client.publish("esphome/discover", None, retain=False)
            dashboard.mqtt_ping_request.wait()
            dashboard.mqtt_ping_request.clear()

        client.disconnect()
        client.loop_stop()
