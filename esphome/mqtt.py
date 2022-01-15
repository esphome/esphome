from datetime import datetime
import hashlib
import logging
import ssl
import sys
import time

import paho.mqtt.client as mqtt

from esphome.const import (
    CONF_BROKER,
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
from esphome.log import color, Fore
from esphome.util import safe_print

_LOGGER = logging.getLogger(__name__)


def initialize(config, subscriptions, on_message, username, password, client_id):
    def on_connect(client, userdata, flags, return_code):
        _LOGGER.info("Connected to MQTT broker!")
        for topic in subscriptions:
            client.subscribe(topic)

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

            wait_time = min(2 ** tries, 300)
            _LOGGER.warning(
                "Disconnected from MQTT (%s). Trying to reconnect in %s s",
                result_code,
                wait_time,
            )
            time.sleep(wait_time)
            tries += 1

    client = mqtt.Client(client_id or "")
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    if username is None:
        if config[CONF_MQTT].get(CONF_USERNAME):
            client.username_pw_set(
                config[CONF_MQTT][CONF_USERNAME], config[CONF_MQTT][CONF_PASSWORD]
            )
    elif username:
        client.username_pw_set(username, password)

    if config[CONF_MQTT].get(CONF_SSL_FINGERPRINTS):
        if sys.version_info >= (2, 7, 13):
            tls_version = ssl.PROTOCOL_TLS  # pylint: disable=no-member
        else:
            tls_version = ssl.PROTOCOL_SSLv23
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

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        pass
    return 0


def show_logs(config, topic=None, username=None, password=None, client_id=None):
    if topic is not None:
        pass  # already have topic
    elif CONF_MQTT in config:
        conf = config[CONF_MQTT]
        if CONF_LOG_TOPIC in conf:
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

    return initialize(config, [topic], on_message, username, password, client_id)


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

    return initialize(config, [topic], on_message, username, password, client_id)


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
