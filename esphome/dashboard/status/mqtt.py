from __future__ import annotations

import binascii
import json
import os
import threading

from esphome import mqtt

from ..core import DASHBOARD


class MqttStatusThread(threading.Thread):
    """Status thread to get the status of the devices via MQTT."""

    def run(self) -> None:
        """Run the status thread."""
        dashboard = DASHBOARD
        entries = dashboard.entries.all()

        config = mqtt.config_from_env()
        topic = "esphome/discover/#"

        def on_message(client, userdata, msg):
            nonlocal entries

            payload = msg.payload.decode(errors="backslashreplace")
            if len(payload) > 0:
                data = json.loads(payload)
                if "name" not in data:
                    return
                for entry in entries:
                    if entry.name == data["name"]:
                        dashboard.ping_result[entry.filename] = True
                        return

        def on_connect(client, userdata, flags, return_code):
            client.publish("esphome/discover", None, retain=False)

        mqttid = str(binascii.hexlify(os.urandom(6)).decode())

        client = mqtt.prepare(
            config,
            [topic],
            on_message,
            on_connect,
            None,
            None,
            f"esphome-dashboard-{mqttid}",
        )
        client.loop_start()

        while not dashboard.stop_event.wait(2):
            entries = dashboard.entries.all()

            # will be set to true on on_message
            for entry in entries:
                if entry.no_mdns:
                    dashboard.ping_result[entry.filename] = False

            client.publish("esphome/discover", None, retain=False)
            dashboard.mqtt_ping_request.wait()
            dashboard.mqtt_ping_request.clear()

        client.disconnect()
        client.loop_stop()
