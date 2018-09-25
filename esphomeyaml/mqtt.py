from __future__ import print_function

import hashlib
import logging
import ssl
import sys
from datetime import datetime

import paho.mqtt.client as mqtt

from esphomeyaml import core
from esphomeyaml.const import CONF_BROKER, CONF_DISCOVERY_PREFIX, CONF_ESPHOMEYAML, \
    CONF_LOG_TOPIC, \
    CONF_MQTT, CONF_NAME, CONF_PASSWORD, CONF_PORT, CONF_TOPIC_PREFIX, \
    CONF_USERNAME, CONF_TOPIC, CONF_SSL_FINGERPRINTS
from esphomeyaml.helpers import color

_LOGGER = logging.getLogger(__name__)


def initialize(config, subscriptions, on_message, username, password, client_id):
    def on_connect(client, userdata, flags, return_code):
        for topic in subscriptions:
            client.subscribe(topic)

    client = mqtt.Client(client_id or u'')
    client.on_connect = on_connect
    client.on_message = on_message
    if username is None:
        if config[CONF_MQTT].get(CONF_USERNAME):
            client.username_pw_set(config[CONF_MQTT][CONF_USERNAME],
                                   config[CONF_MQTT][CONF_PASSWORD])
    elif username:
        client.username_pw_set(username, password)

    if config[CONF_MQTT].get(CONF_SSL_FINGERPRINTS):
        if sys.version_info >= (2, 7, 13):
            tls_version = ssl.PROTOCOL_TLS  # pylint: disable=no-member
        else:
            tls_version = ssl.PROTOCOL_SSLv23
        client.tls_set(ca_certs=None, certfile=None, keyfile=None, cert_reqs=ssl.CERT_REQUIRED,
                       tls_version=tls_version, ciphers=None)
    client.connect(config[CONF_MQTT][CONF_BROKER], config[CONF_MQTT][CONF_PORT])

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        pass
    return 0


def show_logs(config, topic=None, username=None, password=None, client_id=None, escape=False):
    if topic is not None:
        pass  # already have topic
    elif CONF_MQTT in config:
        conf = config[CONF_MQTT]
        if CONF_LOG_TOPIC in conf:
            topic = config[CONF_MQTT][CONF_LOG_TOPIC][CONF_TOPIC]
        elif CONF_TOPIC_PREFIX in config[CONF_MQTT]:
            topic = config[CONF_MQTT][CONF_TOPIC_PREFIX] + u'/debug'
        else:
            topic = config[CONF_ESPHOMEYAML][CONF_NAME] + u'/debug'
    else:
        _LOGGER.error(u"MQTT isn't setup, can't start MQTT logs")
        return 1
    _LOGGER.info(u"Starting log output from %s", topic)

    def on_message(client, userdata, msg):
        time = datetime.now().time().strftime(u'[%H:%M:%S]')
        message = time + msg.payload
        if escape:
            message = message.replace('\033', '\\033')
        try:
            print(message)
        except UnicodeEncodeError:
            print(message.encode('ascii', 'backslashreplace'))

    return initialize(config, [topic], on_message, username, password, client_id)


def clear_topic(config, topic, username=None, password=None, client_id=None):
    if topic is None:
        discovery_prefix = config[CONF_MQTT].get(CONF_DISCOVERY_PREFIX, u'homeassistant')
        name = config[CONF_ESPHOMEYAML][CONF_NAME]
        topic = u'{}/+/{}/#'.format(discovery_prefix, name)
    _LOGGER.info(u"Clearing messages from %s", topic)

    def on_message(client, userdata, msg):
        if not msg.payload or not msg.retain:
            return
        try:
            print(u"Clearing topic {}".format(msg.topic))
        except UnicodeDecodeError:
            print(u"Skipping non-UTF-8 topic (prohibited by MQTT standard)")
            return
        client.publish(msg.topic, None, retain=True)

    return initialize(config, [topic], on_message, username, password, client_id)


# From marvinroger/async-mqtt-client -> scripts/get-fingerprint/get-fingerprint.py
def get_fingerprint(config):
    addr = config[CONF_MQTT][CONF_BROKER], config[CONF_MQTT][CONF_PORT]
    _LOGGER.info("Getting fingerprint from %s:%s", addr[0], addr[1])
    try:
        cert_pem = ssl.get_server_certificate(addr)
    except IOError as err:
        _LOGGER.error("Unable to connect to server: %s", err)
        return 1
    cert_der = ssl.PEM_cert_to_DER_cert(cert_pem)

    sha1 = hashlib.sha1(cert_der).hexdigest()

    print(u"SHA1 Fingerprint: " + color('cyan', sha1))
    print(u"Copy above string into mqtt.ssl_fingerprints section of {}".format(core.CONFIG_PATH))
    return 0
