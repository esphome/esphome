from datetime import datetime
import hashlib
import json
import logging
import ssl
import time

import paho.mqtt.client as mqtt

from esphome.const import (
    CONF_BROKER,
    CONF_CERTIFICATE_AUTHORITY,
    CONF_DISCOVERY_PREFIX,
    CONF_ESPHOME,
    CONF_LOG_TOPIC,
    CONF_MQTT,
    CONF_NAME,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_SSL_FINGERPRINTS,
    CONF_TOPIC,
    CONF_TOPIC_PREFIX,
    CONF_USERNAME,
)
from esphome.core import CORE, EsphomeError
from esphome.helpers import get_int_env, get_str_env
from esphome.log import Fore, color
from esphome.util import safe_print

_LOGGER = logging.getLogger(__name__)


def config_from_env():
    config = {
        CONF_MQTT: {
            CONF_USERNAME: get_str_env("ESPHOME_DASHBOARD_MQTT_USERNAME"),
            CONF_PASSWORD: get_str_env("ESPHOME_DASHBOARD_MQTT_PASSWORD"),
            CONF_BROKER: get_str_env("ESPHOME_DASHBOARD_MQTT_BROKER"),
            CONF_PORT: get_int_env("ESPHOME_DASHBOARD_MQTT_PORT", 1883),
        },
    }
    return config


def initialize(
    config, subscriptions, on_message, on_connect, username, password, client_id
):
    client = prepare(
        config, subscriptions, on_message, on_connect, username, password, client_id
    )
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        pass
    return 0


def prepare(
    config, subscriptions, on_message, on_connect, username, password, client_id
):
    def on_connect_(client, userdata, flags, return_code):
        _LOGGER.info("Connected to MQTT broker!")
        for topic in subscriptions:
            client.subscribe(topic)
        if on_connect is not None:
            on_connect(client, userdata, flags, return_code)

    def on_disconnect(client, userdata, result_code):
        if result_code == 0:
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
                "Disconnected from MQTT (%s). Trying to reconnect in %s s",
                result_code,
                wait_time,
            )
            time.sleep(wait_time)
            tries += 1

    client = mqtt.Client(client_id or "")
    client.on_connect = on_connect_
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    if username is None:
        if config[CONF_MQTT].get(CONF_USERNAME):
            client.username_pw_set(
                config[CONF_MQTT][CONF_USERNAME], config[CONF_MQTT][CONF_PASSWORD]
            )
    elif username:
        client.username_pw_set(username, password)

    if config[CONF_MQTT].get(CONF_SSL_FINGERPRINTS) or config[CONF_MQTT].get(
        CONF_CERTIFICATE_AUTHORITY
    ):
        tls_version = ssl.PROTOCOL_TLS  # pylint: disable=no-member
        client.tls_set(
            ca_certs=None,
            certfile=None,
            keyfile=None,
            cert_reqs=ssl.CERT_REQUIRED,
            tls_version=tls_version,
            ciphers=None,
        )

    try:
        host = str(config[CONF_MQTT][CONF_BROKER])
        port = int(config[CONF_MQTT][CONF_PORT])
        client.connect(host, port)
    except OSError as err:
        raise EsphomeError(f"Cannot connect to MQTT broker: {err}") from err

    return client


def show_discover(config, username=None, password=None, client_id=None):
    topic = "esphome/discover/#"
    _LOGGER.info("Starting log output from %s", topic)

    def on_message(client, userdata, msg):
        time_ = datetime.now().time().strftime("[%H:%M:%S]")
        payload = msg.payload.decode(errors="backslashreplace")
        if len(payload) > 0:
            message = time_ + " " + payload
            safe_print(message)

    def on_connect(client, userdata, flags, return_code):
        _LOGGER.info("Send discover via MQTT broker")
        client.publish("esphome/discover", None, retain=False)

    return initialize(
        config, [topic], on_message, on_connect, username, password, client_id
    )


def get_esphome_device_ip(
    config, username=None, password=None, client_id=None, timeout=25
):
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

    dev_name = config[CONF_ESPHOME][CONF_NAME]
    dev_ip = None

    topic = "esphome/discover/" + dev_name
    _LOGGER.info("Starting looking for IP in topic %s", topic)

    def on_message(client, userdata, msg):
        nonlocal dev_ip
        time_ = datetime.now().time().strftime("[%H:%M:%S]")
        payload = msg.payload.decode(errors="backslashreplace")
        if len(payload) > 0:
            message = time_ + " " + payload
            _LOGGER.debug(message)

            data = json.loads(payload)
            if "name" not in data or data["name"] != dev_name:
                _LOGGER.Warn("Wrong device answer")
                return

            dev_ip = []
            key = "ip"
            n = 0
            while key in data:
                dev_ip.append(data[key])
                n = n + 1
                key = "ip" + str(n)

            if dev_ip:
                client.disconnect()

    def on_connect(client, userdata, flags, return_code):
        topic = "esphome/ping/" + dev_name
        _LOGGER.info("Send discover via MQTT broker topic: %s", topic)
        client.publish(topic, None, retain=False)

    mqtt_client = prepare(
        config, [topic], on_message, on_connect, username, password, client_id
    )

    mqtt_client.loop_start()
    while timeout > 0:
        if dev_ip is not None:
            break
        timeout -= 0.250
        time.sleep(0.250)
    mqtt_client.loop_stop()

    if dev_ip is None:
        raise EsphomeError("Failed to find IP via MQTT")

    _LOGGER.info("Found IP: %s", dev_ip)
    return dev_ip


def show_logs(config, topic=None, username=None, password=None, client_id=None):
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
    _LOGGER.info("Starting log output from %s", topic)

    def on_message(client, userdata, msg):
        time_ = datetime.now().time().strftime("[%H:%M:%S]")
        payload = msg.payload.decode(errors="backslashreplace")
        message = time_ + payload
        safe_print(message)

    return initialize(config, [topic], on_message, None, username, password, client_id)


def clear_topic(config, topic, username=None, password=None, client_id=None):
    if topic is None:
        discovery_prefix = config[CONF_MQTT].get(CONF_DISCOVERY_PREFIX, "homeassistant")
        name = config[CONF_ESPHOME][CONF_NAME]
        topic = f"{discovery_prefix}/+/{name}/#"
    _LOGGER.info("Clearing messages from '%s'", topic)
    _LOGGER.info(
        "Please close this window when no more messages appear and the "
        "MQTT topic has been cleared of retained messages."
    )

    def on_message(client, userdata, msg):
        if not msg.payload or not msg.retain:
            return
        try:
            print(f"Clearing topic {msg.topic}")
        except UnicodeDecodeError:
            print("Skipping non-UTF-8 topic (prohibited by MQTT standard)")
            return
        client.publish(msg.topic, None, retain=True)

    return initialize(config, [topic], on_message, None, username, password, client_id)


# From marvinroger/async-mqtt-client -> scripts/get-fingerprint/get-fingerprint.py
def get_fingerprint(config):
    addr = str(config[CONF_MQTT][CONF_BROKER]), int(config[CONF_MQTT][CONF_PORT])
    _LOGGER.info("Getting fingerprint from %s:%s", addr[0], addr[1])
    try:
        cert_pem = ssl.get_server_certificate(addr)
    except OSError as err:
        _LOGGER.error("Unable to connect to server: %s", err)
        return 1
    cert_der = ssl.PEM_cert_to_DER_cert(cert_pem)

    sha1 = hashlib.sha1(cert_der).hexdigest()

    safe_print(f"SHA1 Fingerprint: {color(Fore.CYAN, sha1)}")
    safe_print(
        f"Copy the string above into mqtt.ssl_fingerprints section of {CORE.config_path}"
    )
    return 0
