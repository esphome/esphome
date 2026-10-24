from __future__ import annotations

import contextlib
from datetime import datetime
import json
import logging
from pathlib import Path
import re
import ssl
import tempfile
import time
from typing import TYPE_CHECKING, Any

import paho.mqtt.client as mqtt

from esphome.const import (
    CONF_BIRTH_MESSAGE,
    CONF_BROKER,
    CONF_CERTIFICATE_AUTHORITY,
    CONF_CLIENT_CERTIFICATE,
    CONF_CLIENT_CERTIFICATE_KEY,
    CONF_DISCOVERY_PREFIX,
    CONF_ESPHOME,
    CONF_LOG_TOPIC,
    CONF_MQTT,
    CONF_NAME,
    CONF_NAME_ADD_MAC_SUFFIX,
    CONF_PASSWORD,
    CONF_PAYLOAD,
    CONF_PORT,
    CONF_RETAIN,
    CONF_SKIP_CERT_CN_CHECK,
    CONF_TOPIC,
    CONF_TOPIC_PREFIX,
    CONF_USERNAME,
)
from esphome.core import EsphomeError
from esphome.helpers import get_int_env, get_str_env
from esphome.types import ConfigType
from esphome.util import safe_print

if TYPE_CHECKING:
    from collections.abc import Callable
    import threading

    from paho.mqtt.properties import Properties
    from paho.mqtt.reasoncodes import ReasonCode

    OnMessage = Callable[[mqtt.Client, Any, mqtt.MQTTMessage], None]
    OnConnect = Callable[
        [mqtt.Client, Any, mqtt.ConnectFlags, ReasonCode, Properties | None], None
    ]

_LOGGER = logging.getLogger(__name__)


def config_from_env() -> ConfigType:
    return {
        CONF_MQTT: {
            CONF_USERNAME: get_str_env("ESPHOME_DASHBOARD_MQTT_USERNAME"),
            CONF_PASSWORD: get_str_env("ESPHOME_DASHBOARD_MQTT_PASSWORD"),
            CONF_BROKER: get_str_env("ESPHOME_DASHBOARD_MQTT_BROKER"),
            CONF_PORT: get_int_env("ESPHOME_DASHBOARD_MQTT_PORT", 1883),
        },
    }


def initialize(
    config: ConfigType,
    subscriptions: list[str],
    on_message: OnMessage,
    on_connect: OnConnect | None,
    username: str | None,
    password: str | None,
    client_id: str | None,
) -> int:
    client = prepare(
        config, subscriptions, on_message, on_connect, username, password, client_id
    )
    with contextlib.suppress(KeyboardInterrupt):
        client.loop_forever()
    return 0


def prepare(
    config: ConfigType,
    subscriptions: list[str],
    on_message: OnMessage,
    on_connect: OnConnect | None,
    username: str | None,
    password: str | None,
    client_id: str | None,
) -> mqtt.Client:
    def on_connect_(
        client: mqtt.Client,
        userdata: Any,
        flags: mqtt.ConnectFlags,
        reason_code: ReasonCode,
        properties: Properties | None,
    ) -> None:
        _LOGGER.info("Connected to MQTT broker!")
        for topic in subscriptions:
            client.subscribe(topic)
        if on_connect is not None:
            on_connect(client, userdata, flags, reason_code, properties)

    def on_disconnect(
        client: mqtt.Client,
        userdata: Any,
        flags: mqtt.DisconnectFlags,
        reason_code: ReasonCode,
        properties: Properties | None,
    ) -> None:
        if reason_code == 0:
            return

        tries = 0
        while True:
            try:
                if client.reconnect() == 0:
                    _LOGGER.info("Successfully reconnected to the MQTT server")
                    break
            except OSError:
                pass

            wait_time = min(2**tries, 300)
            _LOGGER.warning(
                "Disconnected from MQTT (%s, rc=%s). Trying to reconnect in %s s",
                reason_code,
                reason_code.value,
                wait_time,
            )
            time.sleep(wait_time)
            tries += 1

    # Match the firmware's transport; imported here to keep component code off the CLI start-up path
    from esphome.components.mqtt import (
        CONF_TLS,
        CONF_TRANSPORT,
        CONF_VERIFY_SSL,
        CONF_WS_PATH,
        DEFAULT_WS_PATH,
        MQTT_TRANSPORT_TCP,
    )

    transport = config[CONF_MQTT].get(CONF_TRANSPORT, MQTT_TRANSPORT_TCP)
    websocket = transport != MQTT_TRANSPORT_TCP
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id or "",
        transport="websockets" if websocket else "tcp",
    )
    client.on_connect = on_connect_
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    if websocket:
        client.ws_set_options(path=config[CONF_MQTT].get(CONF_WS_PATH, DEFAULT_WS_PATH))
    if username is None:
        if config[CONF_MQTT].get(CONF_USERNAME):
            client.username_pw_set(
                config[CONF_MQTT][CONF_USERNAME], config[CONF_MQTT][CONF_PASSWORD]
            )
    elif username:
        client.username_pw_set(username, password)

    mqtt_conf = config[CONF_MQTT]
    # Validation sets tls whenever a certificate option or the wss transport implies it
    if mqtt_conf.get(CONF_TLS):
        if mqtt_conf.get(CONF_VERIFY_SSL) is False:
            context = ssl.create_default_context()
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
        else:
            # Without a CA the system trust store stands in for the firmware's bundle
            context = ssl.create_default_context(
                cadata=mqtt_conf.get(CONF_CERTIFICATE_AUTHORITY)
            )
            if mqtt_conf.get(CONF_SKIP_CERT_CN_CHECK):
                context.check_hostname = False
        if mqtt_conf.get(CONF_CLIENT_CERTIFICATE):
            with (
                tempfile.NamedTemporaryFile(
                    encoding="utf-8", mode="w+", delete=False
                ) as cert_file,
                tempfile.NamedTemporaryFile(
                    encoding="utf-8", mode="w+", delete=False
                ) as key_file,
            ):
                try:
                    cert_file.write(mqtt_conf[CONF_CLIENT_CERTIFICATE])
                    key_file.write(mqtt_conf[CONF_CLIENT_CERTIFICATE_KEY])
                    cert_file.close()
                    key_file.close()
                    context.load_cert_chain(cert_file.name, key_file.name)
                finally:
                    Path(cert_file.name).unlink()
                    Path(key_file.name).unlink()
        client.tls_set_context(context)

    try:
        host = str(config[CONF_MQTT][CONF_BROKER])
        port = int(config[CONF_MQTT][CONF_PORT])
        client.connect(host, port)
    except OSError as err:
        raise EsphomeError(f"Cannot connect to MQTT broker: {err}") from err

    return client


def show_discover(
    config: ConfigType,
    username: str | None = None,
    password: str | None = None,
    client_id: str | None = None,
) -> int:
    topic = "esphome/discover/#"
    _LOGGER.info("Starting log output from %s", topic)

    def on_message(client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
        time_ = datetime.now().astimezone().time().strftime("[%H:%M:%S]")
        payload = msg.payload.decode(errors="backslashreplace")
        if len(payload) > 0:
            message = time_ + " " + payload
            safe_print(message)

    def on_connect(
        client: mqtt.Client,
        userdata: Any,
        flags: mqtt.ConnectFlags,
        reason_code: ReasonCode,
        properties: Properties | None,
    ) -> None:
        _LOGGER.info("Send discover via MQTT broker")
        client.publish("esphome/discover", None, retain=False)

    return initialize(
        config, [topic], on_message, on_connect, username, password, client_id
    )


def get_esphome_device_ip(
    config: ConfigType,
    username: str | None = None,
    password: str | None = None,
    client_id: str | None = None,
    timeout: float = 25,
    stop_event: threading.Event | None = None,
) -> list[str]:
    if CONF_MQTT not in config:
        raise EsphomeError(
            "Cannot discover IP via MQTT as the config does not include the mqtt: "
            "component"
        )
    if CONF_ESPHOME not in config or CONF_NAME not in config[CONF_ESPHOME]:
        raise EsphomeError(
            "Cannot discover IP via MQTT as the config does not include the device name: "
            "component"
        )
    if not config[CONF_MQTT].get(CONF_BROKER):
        raise EsphomeError(
            "Cannot discover IP via MQTT as the broker is not configured"
        )

    dev_name = config[CONF_ESPHOME][CONF_NAME]
    dev_ip = None
    failed = False

    topic = "esphome/discover/" + dev_name
    _LOGGER.info("Starting looking for IP in topic %s", topic)

    def on_message(client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
        nonlocal dev_ip, failed
        time_ = datetime.now().astimezone().time().strftime("[%H:%M:%S]")
        payload = msg.payload.decode(errors="backslashreplace")
        if len(payload) > 0:
            message = time_ + " " + payload
            _LOGGER.debug(message)

            try:
                data = json.loads(payload)
            except ValueError:
                data = None
            if not isinstance(data, dict):
                # A raise in this handler would kill paho's network thread
                _LOGGER.warning("Ignoring unparsable discovery payload")
                return
            if "name" not in data or data["name"] != dev_name:
                _LOGGER.warning("Wrong device answer")
                return

            addresses = []
            key = "ip"
            n = 0
            while key in data:
                value = data[key]
                if (
                    isinstance(value, str)
                    and (value := value.strip())
                    and value.isprintable()
                ):
                    addresses.append(value)
                else:
                    # repr-escaped and truncated: must not forge log lines
                    _LOGGER.warning(
                        "Ignoring invalid address in discovery answer: %s",
                        repr(value)[:100],
                    )
                n = n + 1
                key = "ip" + str(n)

            if not addresses:
                _LOGGER.warning("Device answer did not include an IP address")
                failed = True
                return

            dev_ip = addresses
            failed = False  # a complete answer wins over an earlier empty one
            client.disconnect()

    def on_connect(
        client: mqtt.Client,
        userdata: Any,
        flags: mqtt.ConnectFlags,
        reason_code: ReasonCode,
        properties: Properties | None,
    ) -> None:
        topic = "esphome/ping/" + dev_name
        _LOGGER.info("Send discover via MQTT broker topic: %s", topic)
        client.publish(topic, None, retain=False)

    if stop_event is not None and stop_event.is_set():
        # Teardown already started; don't open a broker connection at all
        return []

    def on_disconnect(
        client: mqtt.Client,
        userdata: Any,
        flags: mqtt.DisconnectFlags,
        reason_code: ReasonCode,
        properties: Properties | None,
    ) -> None:
        nonlocal failed
        if reason_code != 0:
            _LOGGER.warning(
                "Disconnected from MQTT broker (%s, rc=%s)",
                reason_code,
                reason_code.value,
            )
            failed = True

    mqtt_client = prepare(
        config, [topic], on_message, on_connect, username, password, client_id
    )
    # Discovery is one-shot; prepare()'s reconnect-forever on_disconnect runs
    # on the network thread and would make loop_stop() below join forever.
    mqtt_client.on_disconnect = on_disconnect

    if stop_event is None:
        import threading

        stop_event = threading.Event()  # never set; wait() below is a plain sleep
    stopped = stop_event.is_set()  # teardown may have started during connect
    try:
        if not stopped:
            mqtt_client.loop_start()
            while timeout > 0:
                if dev_ip is not None or failed:
                    break
                if stop_event.wait(0.250):
                    stopped = True
                    break
                timeout -= 0.250
    finally:
        # A cleanup failure must not replace the discovery result or its
        # EsphomeError; a second disconnect after on_message's is harmless.
        try:
            mqtt_client.disconnect()
        except Exception:  # pylint: disable=broad-except
            _LOGGER.debug("Error disconnecting from MQTT broker", exc_info=True)
        mqtt_client.loop_stop()  # only signals and joins; does not raise

    if dev_ip is None:
        if stopped:
            # Aborted by the caller, not a failure; stay quiet
            return []
        raise EsphomeError("Failed to find IP via MQTT")

    _LOGGER.info("Found IP via MQTT broker: %s", ", ".join(dev_ip))
    return dev_ip


def show_logs(
    config: ConfigType,
    topic: str | None = None,
    username: str | None = None,
    password: str | None = None,
    client_id: str | None = None,
) -> int:
    if topic is not None:
        pass  # already have topic
    elif CONF_MQTT in config:
        conf = config[CONF_MQTT]
        if CONF_LOG_TOPIC in conf:
            if config[CONF_MQTT][CONF_LOG_TOPIC] is None:
                _LOGGER.error("MQTT log topic set to null, can't start MQTT logs")
                return 1
            if CONF_TOPIC not in config[CONF_MQTT][CONF_LOG_TOPIC]:
                _LOGGER.error("MQTT log topic not available, can't start MQTT logs")
                return 1
            topic = config[CONF_MQTT][CONF_LOG_TOPIC][CONF_TOPIC]
        elif CONF_TOPIC_PREFIX in config[CONF_MQTT]:
            topic = f"{config[CONF_MQTT][CONF_TOPIC_PREFIX]}/debug"
        else:
            topic = f"{config[CONF_ESPHOME][CONF_NAME]}/debug"
    else:
        _LOGGER.error("MQTT isn't setup, can't start MQTT logs")
        return 1
    name = config[CONF_ESPHOME][CONF_NAME]
    logs_topic = f"esphome/logs/{name}"
    # Ask when the retained birth message reports online: a request sent while the device is down is
    # lost, and only a retained message tells a CLI that connects later that the device is already up.
    # An empty payload deletes the retained message instead, so it counts as no birth message.
    birth = config.get(CONF_MQTT, {}).get(CONF_BIRTH_MESSAGE) or {}
    birth_payload = birth.get(CONF_PAYLOAD)
    birth_topic = birth.get(CONF_TOPIC) if birth_payload else None
    ask_on_connect = birth_topic is None or not birth[CONF_RETAIN]

    mac_suffix = config[CONF_ESPHOME].get(CONF_NAME_ADD_MAC_SUFFIX, False)

    def suffixed(default_topic: str | None, kind: str) -> re.Pattern[str] | None:
        # The firmware adds its MAC to topics defaulted from the name, which the CLI cannot know
        if not mac_suffix or default_topic != f"{name}/{kind}":
            return None
        return re.compile(rf"({re.escape(name)}-[0-9a-f]{{6}})/{kind}")

    log_pattern = suffixed(topic, "debug")
    birth_pattern = suffixed(birth_topic, "status")
    _LOGGER.info(
        "Starting log output from %s",
        f"{name}-<mac>/debug of every device built from this config"
        if log_pattern
        else topic,
    )

    def request_crash_log(client: mqtt.Client, logs_topic: str = logs_topic) -> None:
        _LOGGER.debug("Requesting the crash log via topic: %s", logs_topic)
        client.publish(logs_topic, None, retain=False)

    def on_message(client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
        payload = msg.payload.decode(errors="backslashreplace")
        if payload == birth_payload:
            if birth_pattern is None:
                if msg.topic == birth_topic:
                    request_crash_log(client)
            elif match := birth_pattern.fullmatch(msg.topic):
                request_crash_log(client, f"esphome/logs/{match[1]}")
        # The birth topic may sit inside the log subscription, so print by what was asked for
        if log_pattern:
            # Several boards can run this config, so name the one that logged
            if not (match := log_pattern.fullmatch(msg.topic)):
                return
            payload = f"[{match[1]}]{payload}"
        elif not mqtt.topic_matches_sub(topic, msg.topic):
            return
        time_ = datetime.now().astimezone().time().strftime("[%H:%M:%S]")
        safe_print(time_ + payload)

    def on_connect(
        client: mqtt.Client,
        userdata: Any,
        flags: mqtt.ConnectFlags,
        reason_code: ReasonCode,
        properties: Properties | None,
    ) -> None:
        if ask_on_connect:
            request_crash_log(client)

    subscriptions = ["+/debug" if log_pattern else topic]
    if birth_topic is not None:
        subscriptions.append("+/status" if birth_pattern else birth_topic)
    return initialize(
        config, subscriptions, on_message, on_connect, username, password, client_id
    )


def clear_topic(
    config: ConfigType,
    topic: str | None,
    username: str | None = None,
    password: str | None = None,
    client_id: str | None = None,
) -> int:
    if topic is None:
        discovery_prefix = config[CONF_MQTT].get(CONF_DISCOVERY_PREFIX, "homeassistant")
        name = config[CONF_ESPHOME][CONF_NAME]
        topic = f"{discovery_prefix}/+/{name}/#"
    _LOGGER.info("Clearing messages from '%s'", topic)
    _LOGGER.info(
        "Please close this window when no more messages appear and the "
        "MQTT topic has been cleared of retained messages."
    )

    def on_message(client: mqtt.Client, userdata: Any, msg: mqtt.MQTTMessage) -> None:
        if not msg.payload or not msg.retain:
            return
        try:
            print(f"Clearing topic {msg.topic}")
        except UnicodeDecodeError:
            print("Skipping non-UTF-8 topic (prohibited by MQTT standard)")
            return
        client.publish(msg.topic, None, retain=True)

    return initialize(config, [topic], on_message, None, username, password, client_id)
