from collections import OrderedDict
import re

import voluptuous as vol

from esphome import automation
from esphome.automation import ACTION_REGISTRY
from esphome.components import logger
import esphome.config_validation as cv
from esphome.const import CONF_AVAILABILITY, CONF_BIRTH_MESSAGE, CONF_BROKER, CONF_CLIENT_ID, \
    CONF_COMMAND_TOPIC, CONF_DISCOVERY, CONF_DISCOVERY_PREFIX, CONF_DISCOVERY_RETAIN, \
    CONF_ESPHOME, CONF_ID, CONF_INTERNAL, CONF_KEEPALIVE, CONF_LEVEL, CONF_LOG_TOPIC, \
    CONF_MQTT, CONF_NAME, CONF_ON_JSON_MESSAGE, CONF_ON_MESSAGE, CONF_PASSWORD, CONF_PAYLOAD, \
    CONF_PAYLOAD_AVAILABLE, CONF_PAYLOAD_NOT_AVAILABLE, CONF_PORT, CONF_QOS, CONF_REBOOT_TIMEOUT, \
    CONF_RETAIN, CONF_SHUTDOWN_MESSAGE, CONF_SSL_FINGERPRINTS, CONF_STATE_TOPIC, CONF_TOPIC, \
    CONF_TOPIC_PREFIX, CONF_TRIGGER_ID, CONF_USERNAME, CONF_WILL_MESSAGE
from esphome.core import EsphomeError
from esphome.cpp_generator import Pvariable, RawExpression, StructInitializer, TemplateArguments, \
    add, get_variable, process_lambda, templatable
from esphome.cpp_types import Action, App, Component, JsonObjectConstRef, JsonObjectRef, \
    Trigger, bool_, esphome_ns, optional, std_string, uint8, void


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

mqtt_ns = esphome_ns.namespace('mqtt')
MQTTMessage = mqtt_ns.struct('MQTTMessage')
MQTTClientComponent = mqtt_ns.class_('MQTTClientComponent', Component)
MQTTPublishAction = mqtt_ns.class_('MQTTPublishAction', Action)
MQTTPublishJsonAction = mqtt_ns.class_('MQTTPublishJsonAction', Action)
MQTTMessageTrigger = mqtt_ns.class_('MQTTMessageTrigger', Trigger.template(std_string))
MQTTJsonMessageTrigger = mqtt_ns.class_('MQTTJsonMessageTrigger',
                                        Trigger.template(JsonObjectConstRef))
MQTTComponent = mqtt_ns.class_('MQTTComponent', Component)


def validate_config(value):
    if CONF_PORT not in value:
        parts = value[CONF_BROKER].split(u':')
        if len(parts) == 2:
            value[CONF_BROKER] = parts[0]
            value[CONF_PORT] = cv.port(parts[1])
        else:
            value[CONF_PORT] = 1883
    return value


def validate_fingerprint(value):
    value = cv.string(value)
    if re.match(r'^[0-9a-f]{40}$', value) is None:
        raise vol.Invalid(u"fingerprint must be valid SHA1 hash")
    return value


CONFIG_SCHEMA = vol.All(vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(MQTTClientComponent),
    vol.Required(CONF_BROKER): cv.string_strict,
    vol.Optional(CONF_PORT): cv.port,
    vol.Optional(CONF_USERNAME, default=''): cv.string,
    vol.Optional(CONF_PASSWORD, default=''): cv.string,
    vol.Optional(CONF_CLIENT_ID): vol.All(cv.string, vol.Length(max=23)),
    vol.Optional(CONF_DISCOVERY): vol.Any(cv.boolean, cv.one_of("CLEAN", upper=True)),
    vol.Optional(CONF_DISCOVERY_RETAIN): cv.boolean,
    vol.Optional(CONF_DISCOVERY_PREFIX): cv.publish_topic,
    vol.Optional(CONF_BIRTH_MESSAGE): MQTT_MESSAGE_SCHEMA,
    vol.Optional(CONF_WILL_MESSAGE): MQTT_MESSAGE_SCHEMA,
    vol.Optional(CONF_SHUTDOWN_MESSAGE): MQTT_MESSAGE_SCHEMA,
    vol.Optional(CONF_TOPIC_PREFIX): cv.publish_topic,
    vol.Optional(CONF_LOG_TOPIC): vol.Any(None, MQTT_MESSAGE_BASE.extend({
        vol.Optional(CONF_LEVEL): logger.is_log_level,
    }), validate_message_just_topic),
    vol.Optional(CONF_SSL_FINGERPRINTS): vol.All(cv.only_on_esp8266,
                                                 cv.ensure_list(validate_fingerprint)),
    vol.Optional(CONF_KEEPALIVE): cv.positive_time_period_seconds,
    vol.Optional(CONF_REBOOT_TIMEOUT): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_ON_MESSAGE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(MQTTMessageTrigger),
        vol.Required(CONF_TOPIC): cv.subscribe_topic,
        vol.Optional(CONF_QOS): cv.mqtt_qos,
        vol.Optional(CONF_PAYLOAD): cv.string_strict,
    }),
    vol.Optional(CONF_ON_JSON_MESSAGE): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(MQTTJsonMessageTrigger),
        vol.Required(CONF_TOPIC): cv.subscribe_topic,
        vol.Optional(CONF_QOS, default=0): cv.mqtt_qos,
    }),
}), validate_config)


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

    discovery = config.get(CONF_DISCOVERY, True)
    discovery_retain = config.get(CONF_DISCOVERY_RETAIN, True)
    discovery_prefix = config.get(CONF_DISCOVERY_PREFIX, 'homeassistant')

    if not discovery:
        add(mqtt.disable_discovery())
    elif discovery == "CLEAN":
        add(mqtt.set_discovery_info(discovery_prefix, discovery_retain, True))
    elif CONF_DISCOVERY_RETAIN in config or CONF_DISCOVERY_PREFIX in config:
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

            if CONF_LEVEL in log_topic:
                add(mqtt.set_log_level(logger.LOG_LEVELS[log_topic[CONF_LEVEL]]))

    if CONF_SSL_FINGERPRINTS in config:
        for fingerprint in config[CONF_SSL_FINGERPRINTS]:
            arr = [RawExpression("0x{}".format(fingerprint[i:i + 2])) for i in range(0, 40, 2)]
            add(mqtt.add_ssl_fingerprint(arr))

    if CONF_KEEPALIVE in config:
        add(mqtt.set_keep_alive(config[CONF_KEEPALIVE]))

    if CONF_REBOOT_TIMEOUT in config:
        add(mqtt.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))

    for conf in config.get(CONF_ON_MESSAGE, []):
        rhs = App.register_component(mqtt.make_message_trigger(conf[CONF_TOPIC]))
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        if CONF_QOS in conf:
            add(trigger.set_qos(conf[CONF_QOS]))
        if CONF_PAYLOAD in conf:
            add(trigger.set_payload(conf[CONF_PAYLOAD]))
        automation.build_automation(trigger, std_string, conf)

    for conf in config.get(CONF_ON_JSON_MESSAGE, []):
        rhs = mqtt.make_json_message_trigger(conf[CONF_TOPIC], conf[CONF_QOS])
        trigger = Pvariable(conf[CONF_TRIGGER_ID], rhs)
        automation.build_automation(trigger, JsonObjectConstRef, conf)


CONF_MQTT_PUBLISH = 'mqtt.publish'
MQTT_PUBLISH_ACTION_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.use_variable_id(MQTTClientComponent),
    vol.Required(CONF_TOPIC): cv.templatable(cv.publish_topic),
    vol.Required(CONF_PAYLOAD): cv.templatable(cv.mqtt_payload),
    vol.Optional(CONF_QOS): cv.templatable(cv.mqtt_qos),
    vol.Optional(CONF_RETAIN): cv.templatable(cv.boolean),
})


@ACTION_REGISTRY.register(CONF_MQTT_PUBLISH, MQTT_PUBLISH_ACTION_SCHEMA)
def mqtt_publish_action_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_publish_action(template_arg)
    type = MQTTPublishAction.template(template_arg)
    action = Pvariable(action_id, rhs, type=type)
    for template_ in templatable(config[CONF_TOPIC], arg_type, std_string):
        yield None
    add(action.set_topic(template_))

    for template_ in templatable(config[CONF_PAYLOAD], arg_type, std_string):
        yield None
    add(action.set_payload(template_))
    if CONF_QOS in config:
        for template_ in templatable(config[CONF_QOS], arg_type, uint8):
            yield
        add(action.set_qos(template_))
    if CONF_RETAIN in config:
        for template_ in templatable(config[CONF_RETAIN], arg_type, bool_):
            yield None
        add(action.set_retain(template_))
    yield action


CONF_MQTT_PUBLISH_JSON = 'mqtt.publish_json'
MQTT_PUBLISH_JSON_ACTION_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.use_variable_id(MQTTClientComponent),
    vol.Required(CONF_TOPIC): cv.templatable(cv.publish_topic),
    vol.Required(CONF_PAYLOAD): cv.lambda_,
    vol.Optional(CONF_QOS): cv.mqtt_qos,
    vol.Optional(CONF_RETAIN): cv.boolean,
})


@ACTION_REGISTRY.register(CONF_MQTT_PUBLISH_JSON, MQTT_PUBLISH_JSON_ACTION_SCHEMA)
def mqtt_publish_json_action_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_publish_json_action(template_arg)
    type = MQTTPublishJsonAction.template(template_arg)
    action = Pvariable(action_id, rhs, type=type)
    for template_ in templatable(config[CONF_TOPIC], arg_type, std_string):
        yield None
    add(action.set_topic(template_))

    for lambda_ in process_lambda(config[CONF_PAYLOAD], [(arg_type, 'x'), (JsonObjectRef, 'root')],
                                  return_type=void):
        yield None
    add(action.set_payload(lambda_))
    if CONF_QOS in config:
        add(action.set_qos(config[CONF_QOS]))
    if CONF_RETAIN in config:
        add(action.set_retain(config[CONF_RETAIN]))
    yield action


def required_build_flags(config):
    if CONF_SSL_FINGERPRINTS in config:
        return '-DASYNC_TCP_SSL_ENABLED=1'
    return None


def get_default_topic_for(data, component_type, name, suffix):
    whitelist = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_'
    sanitized_name = ''.join(x for x in name.lower().replace(' ', '_') if x in whitelist)
    return '{}/{}/{}/{}'.format(data.topic_prefix, component_type,
                                sanitized_name, suffix)


def build_hass_config(data, component_type, config, include_state=True, include_command=True):
    if config.get(CONF_INTERNAL, False):
        return None
    ret = OrderedDict()
    ret['platform'] = 'mqtt'
    ret['name'] = config[CONF_NAME]
    if include_state:
        default = get_default_topic_for(data, component_type, config[CONF_NAME], 'state')
        ret['state_topic'] = config.get(CONF_STATE_TOPIC, default)
    if include_command:
        default = get_default_topic_for(data, component_type, config[CONF_NAME], 'command')
        ret['command_topic'] = config.get(CONF_STATE_TOPIC, default)
    avail = config.get(CONF_AVAILABILITY, data.availability)
    if avail:
        ret['availability_topic'] = avail[CONF_TOPIC]
        payload_available = avail[CONF_PAYLOAD_AVAILABLE]
        if payload_available != 'online':
            ret['payload_available'] = payload_available
        payload_not_available = avail[CONF_PAYLOAD_NOT_AVAILABLE]
        if payload_not_available != 'offline':
            ret['payload_not_available'] = payload_not_available
    return ret


class GenerateHassConfigData(object):
    def __init__(self, config):
        if 'mqtt' not in config:
            raise EsphomeError("Cannot generate Home Assistant MQTT config if MQTT is not "
                               "used!")
        mqtt = config[CONF_MQTT]
        self.topic_prefix = mqtt.get(CONF_TOPIC_PREFIX, config[CONF_ESPHOME][CONF_NAME])
        birth_message = mqtt.get(CONF_BIRTH_MESSAGE)
        if CONF_BIRTH_MESSAGE not in mqtt:
            birth_message = {
                CONF_TOPIC: self.topic_prefix + '/status',
                CONF_PAYLOAD: 'online',
            }
        will_message = mqtt.get(CONF_WILL_MESSAGE)
        if CONF_WILL_MESSAGE not in mqtt:
            will_message = {
                CONF_TOPIC: self.topic_prefix + '/status',
                CONF_PAYLOAD: 'offline'
            }
        if not birth_message or not will_message:
            self.availability = None
        elif birth_message[CONF_TOPIC] != will_message[CONF_TOPIC]:
            self.availability = None
        else:
            self.availability = {
                CONF_TOPIC: birth_message[CONF_TOPIC],
                CONF_PAYLOAD_AVAILABLE: birth_message[CONF_PAYLOAD],
                CONF_PAYLOAD_NOT_AVAILABLE: will_message[CONF_PAYLOAD],
            }


def setup_mqtt_component(obj, config):
    if CONF_RETAIN in config:
        add(obj.set_retain(config[CONF_RETAIN]))
    if not config.get(CONF_DISCOVERY, True):
        add(obj.disable_discovery())
    if CONF_STATE_TOPIC in config:
        add(obj.set_custom_state_topic(config[CONF_STATE_TOPIC]))
    if CONF_COMMAND_TOPIC in config:
        add(obj.set_custom_command_topic(config[CONF_COMMAND_TOPIC]))
    if CONF_AVAILABILITY in config:
        availability = config[CONF_AVAILABILITY]
        if not availability:
            add(obj.disable_availability())
        else:
            add(obj.set_availability(availability[CONF_TOPIC], availability[CONF_PAYLOAD_AVAILABLE],
                                     availability[CONF_PAYLOAD_NOT_AVAILABLE]))


LIB_DEPS = 'AsyncMqttClient@0.8.2'
BUILD_FLAGS = '-DUSE_MQTT'
