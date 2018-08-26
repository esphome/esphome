import re

import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import automation
from esphomeyaml.const import CONF_BIRTH_MESSAGE, CONF_BROKER, CONF_CLIENT_ID, CONF_DISCOVERY, \
    CONF_DISCOVERY_PREFIX, CONF_DISCOVERY_RETAIN, CONF_ID, CONF_KEEPALIVE, CONF_LOG_TOPIC, \
    CONF_ON_MESSAGE, CONF_PASSWORD, CONF_PAYLOAD, CONF_PORT, CONF_QOS, CONF_RETAIN, \
    CONF_SSL_FINGERPRINTS, CONF_TOPIC, CONF_TOPIC_PREFIX, CONF_TRIGGER_ID, CONF_USERNAME, \
    CONF_WILL_MESSAGE, CONF_REBOOT_TIMEOUT, CONF_SHUTDOWN_MESSAGE
from esphomeyaml.helpers import App, ArrayInitializer, Pvariable, RawExpression, \
    StructInitializer, \
    TemplateArguments, add, esphomelib_ns, optional, std_string


def validate_message_just_topic(value):
    value = cv.publish_topic(value)
    return MQTT_MESSAGE_BASE({CONF_TOPIC: value})


MQTT_MESSAGE_BASE = vol.Schema({
    vol.Required(CONF_TOPIC): cv.publish_topic,
    vol.Optional(CONF_QOS, default=0): cv.mqtt_qos,
    vol.Optional(CONF_RETAIN, default=True): cv.boolean,
})

MQTT_MESSAGE_TEMPLATE_SCHEMA = vol.Any(None, MQTT_MESSAGE_BASE, validate_message_just_topic)

MQTT_MESSAGE_SCHEMA = vol.Any(None, MQTT_MESSAGE_BASE.extend({
    vol.Required(CONF_PAYLOAD): cv.mqtt_payload,
}))

mqtt_ns = esphomelib_ns.namespace('mqtt')
MQTTMessage = mqtt_ns.MQTTMessage
MQTTClientComponent = mqtt_ns.MQTTClientComponent
MQTTPublishAction = mqtt_ns.MQTTPublishAction
MQTTMessageTrigger = mqtt_ns.MQTTMessageTrigger


def validate_broker(value):
    value = cv.string_strict(value)
    if u':' in value:
        raise vol.Invalid(u"Please specify the port using the port: option")
    if not value:
        raise vol.Invalid(u"Broker cannot be empty")
    return value


def validate_fingerprint(value):
    value = cv.string(value)
    if re.match(r'^[0-9a-f]{40}$', value) is None:
        raise vol.Invalid(u"fingerprint must be valid SHA1 hash")
    return value


CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(MQTTClientComponent),
    vol.Required(CONF_BROKER): validate_broker,
    vol.Optional(CONF_PORT, default=1883): cv.port,
    vol.Optional(CONF_USERNAME, default=''): cv.string,
    vol.Optional(CONF_PASSWORD, default=''): cv.string,
    vol.Optional(CONF_CLIENT_ID): vol.All(cv.string, vol.Length(max=23)),
    vol.Optional(CONF_DISCOVERY): cv.boolean,
    vol.Optional(CONF_DISCOVERY_RETAIN): cv.boolean,
    vol.Optional(CONF_DISCOVERY_PREFIX): cv.publish_topic,
    vol.Optional(CONF_BIRTH_MESSAGE): MQTT_MESSAGE_SCHEMA,
    vol.Optional(CONF_WILL_MESSAGE): MQTT_MESSAGE_SCHEMA,
    vol.Optional(CONF_SHUTDOWN_MESSAGE): MQTT_MESSAGE_SCHEMA,
    vol.Optional(CONF_TOPIC_PREFIX): cv.publish_topic,
    vol.Optional(CONF_LOG_TOPIC): MQTT_MESSAGE_TEMPLATE_SCHEMA,
    vol.Optional(CONF_SSL_FINGERPRINTS): vol.All(cv.only_on_esp8266,
                                                 cv.ensure_list, [validate_fingerprint]),
    vol.Optional(CONF_KEEPALIVE): cv.positive_time_period_seconds,
    vol.Optional(CONF_REBOOT_TIMEOUT): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_ON_MESSAGE): vol.All(cv.ensure_list, [automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(MQTTMessageTrigger),
        vol.Required(CONF_TOPIC): cv.publish_topic,
        vol.Optional(CONF_QOS, default=0): cv.mqtt_qos,
    })])
})


def exp_mqtt_message(config):
    if config is None:
        return optional(TemplateArguments(MQTTMessage))
    exp = StructInitializer(
        MQTTMessage,
        ('topic', config[CONF_TOPIC]),
        ('payload', config.get(CONF_PAYLOAD, "")),
        ('qos', config[CONF_QOS]),
        ('retain', config[CONF_RETAIN])
    )
    return exp


def to_code(config):
    rhs = App.init_mqtt(config[CONF_BROKER], config[CONF_PORT],
                        config[CONF_USERNAME], config[CONF_PASSWORD])
    mqtt = Pvariable(config[CONF_ID], rhs)
    if not config.get(CONF_DISCOVERY, True):
        add(mqtt.disable_discovery())
    if CONF_DISCOVERY_RETAIN in config or CONF_DISCOVERY_PREFIX in config:
        discovery_retain = config.get(CONF_DISCOVERY_RETAIN, True)
        discovery_prefix = config.get(CONF_DISCOVERY_PREFIX, 'homeassistant')
        add(mqtt.set_discovery_info(discovery_prefix, discovery_retain))
    if CONF_TOPIC_PREFIX in config:
        add(mqtt.set_topic_prefix(config[CONF_TOPIC_PREFIX]))
    if CONF_BIRTH_MESSAGE in config:
        birth_message = config[CONF_BIRTH_MESSAGE]
        if not birth_message:
            add(mqtt.disable_birth_message())
        else:
            add(mqtt.set_birth_message(exp_mqtt_message(birth_message)))
    if CONF_WILL_MESSAGE in config:
        will_message = config[CONF_WILL_MESSAGE]
        if not will_message:
            add(mqtt.disable_last_will())
        else:
            add(mqtt.set_last_will(exp_mqtt_message(will_message)))
    if CONF_SHUTDOWN_MESSAGE in config:
        shutdown_message = config[CONF_SHUTDOWN_MESSAGE]
        if not shutdown_message:
            add(mqtt.disable_shutdown_message())
        else:
            add(mqtt.set_shutdown_message(exp_mqtt_message(shutdown_message)))
    if CONF_CLIENT_ID in config:
        add(mqtt.set_client_id(config[CONF_CLIENT_ID]))
    if CONF_LOG_TOPIC in config:
        log_topic = config[CONF_LOG_TOPIC]
        if not log_topic:
            add(mqtt.disable_log_message())
        else:
            add(mqtt.set_log_message_template(exp_mqtt_message(log_topic)))
    if CONF_SSL_FINGERPRINTS in config:
        for fingerprint in config[CONF_SSL_FINGERPRINTS]:
            arr = [RawExpression("0x{}".format(fingerprint[i:i + 2])) for i in range(0, 40, 2)]
            add(mqtt.add_ssl_fingerprint(ArrayInitializer(*arr, multiline=False)))
    if CONF_KEEPALIVE in config:
        add(mqtt.set_keep_alive(config[CONF_KEEPALIVE]))
    if CONF_REBOOT_TIMEOUT in config:
        add(mqtt.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))

    for conf in config.get(CONF_ON_MESSAGE, []):
        rhs = mqtt.make_message_trigger(conf[CONF_TOPIC], conf[CONF_QOS])
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, std_string, conf)


def required_build_flags(config):
    if CONF_SSL_FINGERPRINTS in config:
        return '-DASYNC_TCP_SSL_ENABLED=1'
    return None
