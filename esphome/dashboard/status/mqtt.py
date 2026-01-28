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

    def run(self) -> None:
        """Run the status thread."""
        dashboard = self.dashboard
        entries = dashboard.entries
        current_entries = entries.all()

        config = mqtt.config_from_env()
        topic = "esphome/discover/#"

        def on_message(client, userdata, msg):
            payload = msg.payload.decode(errors="backslashreplace")
            if len(payload) > 0:
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

        client = None
        retry_delay = _CONNECT_RETRY_INITIAL_DELAY
        while client is None and not dashboard.stop_event.is_set():
            try:
                client = mqtt.prepare(
                    config,
                    [topic],
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
                entries.set_state_if_source(
                    entry, bool_to_entry_state(False, EntryStateSource.MQTT)
                )

            client.publish("esphome/discover", None, retain=False)
            dashboard.mqtt_ping_request.wait()
            dashboard.mqtt_ping_request.clear()

        client.disconnect()
        client.loop_stop()
