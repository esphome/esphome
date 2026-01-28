from __future__ import annotations

import binascii
import json
import os
import threading
import typing

from esphome import mqtt

from ..entries import EntryStateSource, bool_to_entry_state

if typing.TYPE_CHECKING:
    from ..core import ESPHomeDashboard


class MqttStatusThread(threading.Thread):
    """Status thread to get the status of the devices via MQTT."""

    def __init__(self, dashboard: ESPHomeDashboard) -> None:
        """Initialize the status thread."""
        super().__init__()
        self.dashboard = dashboard

    @staticmethod
    def _extract_name_from_status_topic(topic: str) -> str | None:
        """Extract an entry name from a default MQTT status topic.

        ESPHome's default MQTT birth/will topic is `<topic_prefix>/status`, and
        by default `topic_prefix` is the node name.
        """
        if not topic.endswith("/status"):
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
            if (status_name := self._extract_name_from_status_topic(msg.topic)) is not None:
                if payload == "online":
                    online_from_status.add(status_name)
                    if matching_entries := entries.get_by_name(status_name):
                        for entry in matching_entries:
                            entries.set_state_if_online_or_source(
                                entry, bool_to_entry_state(True, EntryStateSource.MQTT)
                            )
                elif payload == "offline":
                    online_from_status.discard(status_name)
                    if matching_entries := entries.get_by_name(status_name):
                        for entry in matching_entries:
                            entries.set_state_if_source(
                                entry, bool_to_entry_state(False, EntryStateSource.MQTT)
                            )
                return

            if len(payload) > 0 and msg.topic.startswith("esphome/discover/"):
                data = json.loads(payload)
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

        client = mqtt.prepare(
            config,
            [discover_topic, status_topic],
            on_message,
            on_connect,
            None,
            None,
            f"esphome-dashboard-{mqttid}",
        )
        client.loop_start()

        while not dashboard.stop_event.wait(2):
            current_entries = entries.all()
            # will be set to true on on_message
            for entry in current_entries:
                # Only override state if we don't have a state from another source
                if entry.name not in online_from_status:
                    entries.set_state_if_source(
                        entry, bool_to_entry_state(False, EntryStateSource.MQTT)
                    )

            client.publish("esphome/discover", None, retain=False)
            dashboard.mqtt_ping_request.wait()
            dashboard.mqtt_ping_request.clear()

        client.disconnect()
        client.loop_stop()
