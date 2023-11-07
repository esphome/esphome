import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import Condition
from esphome.components import logger
from esphome.const import (
    CONF_AVAILABILITY,
    CONF_BIRTH_MESSAGE,
    CONF_BROKER,
    CONF_CERTIFICATE_AUTHORITY,
    CONF_CLIENT_ID,
    CONF_COMMAND_TOPIC,
    CONF_COMMAND_RETAIN,
    CONF_DISCOVERY,
    CONF_DISCOVERY_PREFIX,
    CONF_DISCOVERY_RETAIN,
    CONF_DISCOVERY_UNIQUE_ID_GENERATOR,
    CONF_DISCOVERY_OBJECT_ID_GENERATOR,
    CONF_ID,
    CONF_KEEPALIVE,
    CONF_LEVEL,
    CONF_LOG_TOPIC,
    CONF_ON_JSON_MESSAGE,
    CONF_ON_MESSAGE,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_PASSWORD,
    CONF_PAYLOAD,
    CONF_PAYLOAD_AVAILABLE,
    CONF_PAYLOAD_NOT_AVAILABLE,
    CONF_PORT,
    CONF_QOS,
    CONF_REBOOT_TIMEOUT,
    CONF_RETAIN,
    CONF_SHUTDOWN_MESSAGE,
    CONF_SSL_FINGERPRINTS,
    CONF_STATE_TOPIC,
    CONF_TOPIC,
    CONF_TOPIC_PREFIX,
    CONF_TRIGGER_ID,
    CONF_USE_ABBREVIATIONS,
    CONF_USERNAME,
    CONF_WILL_MESSAGE,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_BK72XX,
)
from esphome.core import coroutine_with_priority, CORE
from esphome.components.esp32 import add_idf_sdkconfig_option

DEPENDENCIES = ["network"]

AUTO_LOAD = ["json"]

CONF_IDF_SEND_ASYNC = "idf_send_async"
CONF_SKIP_CERT_CN_CHECK = "skip_cert_cn_check"


def validate_message_just_topic(value):
    value = cv.publish_topic(value)
    return MQTT_MESSAGE_BASE({CONF_TOPIC: value})


MQTT_MESSAGE_BASE = cv.Schema(
    {
        cv.Required(CONF_TOPIC): cv.publish_topic,
        cv.Optional(CONF_QOS, default=0): cv.mqtt_qos,
        cv.Optional(CONF_RETAIN, default=True): cv.boolean,
    }
)

MQTT_MESSAGE_TEMPLATE_SCHEMA = cv.Any(
    None, MQTT_MESSAGE_BASE, validate_message_just_topic
)

MQTT_MESSAGE_SCHEMA = cv.Any(
    None,
    MQTT_MESSAGE_BASE.extend(
        {
            cv.Required(CONF_PAYLOAD): cv.mqtt_payload,
        }
    ),
)

mqtt_ns = cg.esphome_ns.namespace("mqtt")
MQTTMessage = mqtt_ns.struct("MQTTMessage")
MQTTClientComponent = mqtt_ns.class_("MQTTClientComponent", cg.Component)
MQTTPublishAction = mqtt_ns.class_("MQTTPublishAction", automation.Action)
MQTTPublishJsonAction = mqtt_ns.class_("MQTTPublishJsonAction", automation.Action)
MQTTMessageTrigger = mqtt_ns.class_(
    "MQTTMessageTrigger", automation.Trigger.template(cg.std_string), cg.Component
)
MQTTJsonMessageTrigger = mqtt_ns.class_(
    "MQTTJsonMessageTrigger", automation.Trigger.template(cg.JsonObjectConst)
)
MQTTConnectTrigger = mqtt_ns.class_("MQTTConnectTrigger", automation.Trigger.template())
MQTTDisconnectTrigger = mqtt_ns.class_(
    "MQTTDisconnectTrigger", automation.Trigger.template()
)
MQTTComponent = mqtt_ns.class_("MQTTComponent", cg.Component)
MQTTConnectedCondition = mqtt_ns.class_("MQTTConnectedCondition", Condition)

MQTTBinarySensorComponent = mqtt_ns.class_("MQTTBinarySensorComponent", MQTTComponent)
MQTTClimateComponent = mqtt_ns.class_("MQTTClimateComponent", MQTTComponent)
MQTTCoverComponent = mqtt_ns.class_("MQTTCoverComponent", MQTTComponent)
MQTTFanComponent = mqtt_ns.class_("MQTTFanComponent", MQTTComponent)
MQTTJSONLightComponent = mqtt_ns.class_("MQTTJSONLightComponent", MQTTComponent)
MQTTSensorComponent = mqtt_ns.class_("MQTTSensorComponent", MQTTComponent)
MQTTSwitchComponent = mqtt_ns.class_("MQTTSwitchComponent", MQTTComponent)
MQTTTextSensor = mqtt_ns.class_("MQTTTextSensor", MQTTComponent)
MQTTNumberComponent = mqtt_ns.class_("MQTTNumberComponent", MQTTComponent)
MQTTTextComponent = mqtt_ns.class_("MQTTTextComponent", MQTTComponent)
MQTTSelectComponent = mqtt_ns.class_("MQTTSelectComponent", MQTTComponent)
MQTTButtonComponent = mqtt_ns.class_("MQTTButtonComponent", MQTTComponent)
MQTTLockComponent = mqtt_ns.class_("MQTTLockComponent", MQTTComponent)

MQTTDiscoveryUniqueIdGenerator = mqtt_ns.enum("MQTTDiscoveryUniqueIdGenerator")
MQTT_DISCOVERY_UNIQUE_ID_GENERATOR_OPTIONS = {
    "legacy": MQTTDiscoveryUniqueIdGenerator.MQTT_LEGACY_UNIQUE_ID_GENERATOR,
    "mac": MQTTDiscoveryUniqueIdGenerator.MQTT_MAC_ADDRESS_UNIQUE_ID_GENERATOR,
}

MQTTDiscoveryObjectIdGenerator = mqtt_ns.enum("MQTTDiscoveryObjectIdGenerator")
MQTT_DISCOVERY_OBJECT_ID_GENERATOR_OPTIONS = {
    "none": MQTTDiscoveryObjectIdGenerator.MQTT_NONE_OBJECT_ID_GENERATOR,
    "device_name": MQTTDiscoveryObjectIdGenerator.MQTT_DEVICE_NAME_OBJECT_ID_GENERATOR,
}


def validate_config(value):
    # Populate default fields
    out = value.copy()
    topic_prefix = value[CONF_TOPIC_PREFIX]
    # If the topic prefix is not null and these messages are not configured, then set them to the default
    # If the topic prefix is null and these messages are not configured, then set them to null
    if CONF_BIRTH_MESSAGE not in value:
        if topic_prefix != "":
            out[CONF_BIRTH_MESSAGE] = {
                CONF_TOPIC: f"{topic_prefix}/status",
                CONF_PAYLOAD: "online",
                CONF_QOS: 0,
                CONF_RETAIN: True,
            }
        else:
            out[CONF_BIRTH_MESSAGE] = {}
    if CONF_WILL_MESSAGE not in value:
        if topic_prefix != "":
            out[CONF_WILL_MESSAGE] = {
                CONF_TOPIC: f"{topic_prefix}/status",
                CONF_PAYLOAD: "offline",
                CONF_QOS: 0,
                CONF_RETAIN: True,
            }
        else:
            out[CONF_WILL_MESSAGE] = {}
    if CONF_SHUTDOWN_MESSAGE not in value:
        if topic_prefix != "":
            out[CONF_SHUTDOWN_MESSAGE] = {
                CONF_TOPIC: f"{topic_prefix}/status",
                CONF_PAYLOAD: "offline",
                CONF_QOS: 0,
                CONF_RETAIN: True,
            }
        else:
            out[CONF_SHUTDOWN_MESSAGE] = {}
    if CONF_LOG_TOPIC not in value:
        if topic_prefix != "":
            out[CONF_LOG_TOPIC] = {
                CONF_TOPIC: f"{topic_prefix}/debug",
                CONF_QOS: 0,
                CONF_RETAIN: True,
            }
        else:
            out[CONF_LOG_TOPIC] = {}
    return out


def validate_fingerprint(value):
    value = cv.string(value)
    if re.match(r"^[0-9a-f]{40}$", value) is None:
        raise cv.Invalid("fingerprint must be valid SHA1 hash")
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MQTTClientComponent),
            cv.Required(CONF_BROKER): cv.string_strict,
            cv.Optional(CONF_PORT, default=1883): cv.port,
            cv.Optional(CONF_USERNAME, default=""): cv.string,
            cv.Optional(CONF_PASSWORD, default=""): cv.string,
            cv.Optional(CONF_CLIENT_ID): cv.string,
            cv.SplitDefault(CONF_IDF_SEND_ASYNC, esp32_idf=False): cv.All(
                cv.boolean, cv.only_with_esp_idf
            ),
            cv.Optional(CONF_CERTIFICATE_AUTHORITY): cv.All(
                cv.string, cv.only_with_esp_idf
            ),
            cv.SplitDefault(CONF_SKIP_CERT_CN_CHECK, esp32_idf=False): cv.All(
                cv.boolean, cv.only_with_esp_idf
            ),
            cv.Optional(CONF_DISCOVERY, default=True): cv.Any(
                cv.boolean, cv.one_of("CLEAN", upper=True)
            ),
            cv.Optional(CONF_DISCOVERY_RETAIN, default=True): cv.boolean,
            cv.Optional(
                CONF_DISCOVERY_PREFIX, default="homeassistant"
            ): cv.publish_topic,
            cv.Optional(CONF_DISCOVERY_UNIQUE_ID_GENERATOR, default="legacy"): cv.enum(
                MQTT_DISCOVERY_UNIQUE_ID_GENERATOR_OPTIONS
            ),
            cv.Optional(CONF_DISCOVERY_OBJECT_ID_GENERATOR, default="none"): cv.enum(
                MQTT_DISCOVERY_OBJECT_ID_GENERATOR_OPTIONS
            ),
            cv.Optional(CONF_USE_ABBREVIATIONS, default=True): cv.boolean,
            cv.Optional(CONF_BIRTH_MESSAGE): MQTT_MESSAGE_SCHEMA,
            cv.Optional(CONF_WILL_MESSAGE): MQTT_MESSAGE_SCHEMA,
            cv.Optional(CONF_SHUTDOWN_MESSAGE): MQTT_MESSAGE_SCHEMA,
            cv.Optional(CONF_TOPIC_PREFIX, default=lambda: CORE.name): cv.publish_topic,
            cv.Optional(CONF_LOG_TOPIC): cv.Any(
                None,
                MQTT_MESSAGE_BASE.extend(
                    {
                        cv.Optional(CONF_LEVEL): logger.is_log_level,
                    }
                ),
                validate_message_just_topic,
            ),
            cv.Optional(CONF_SSL_FINGERPRINTS): cv.All(
                cv.only_on_esp8266, cv.ensure_list(validate_fingerprint)
            ),
            cv.Optional(CONF_KEEPALIVE, default="15s"): cv.positive_time_period_seconds,
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="15min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MQTTConnectTrigger),
                }
            ),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        MQTTDisconnectTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MQTTMessageTrigger),
                    cv.Required(CONF_TOPIC): cv.subscribe_topic,
                    cv.Optional(CONF_QOS, default=0): cv.mqtt_qos,
                    cv.Optional(CONF_PAYLOAD): cv.string_strict,
                }
            ),
            cv.Optional(CONF_ON_JSON_MESSAGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        MQTTJsonMessageTrigger
                    ),
                    cv.Required(CONF_TOPIC): cv.subscribe_topic,
                    cv.Optional(CONF_QOS, default=0): cv.mqtt_qos,
                }
            ),
        }
    ),
    validate_config,
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_BK72XX]),
)


def exp_mqtt_message(config):
    if config is None:
        return cg.optional(cg.TemplateArguments(MQTTMessage))
    exp = cg.StructInitializer(
        MQTTMessage,
        ("topic", config[CONF_TOPIC]),
        ("payload", config.get(CONF_PAYLOAD, "")),
        ("qos", config[CONF_QOS]),
        ("retain", config[CONF_RETAIN]),
    )
    return exp


@coroutine_with_priority(40.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # Add required libraries for ESP8266 and LibreTiny
    if CORE.is_esp8266 or CORE.is_libretiny:
        # https://github.com/heman/async-mqtt-client/blob/master/library.json
        cg.add_library("heman/AsyncMqttClient-esphome", "2.0.0")

    cg.add_define("USE_MQTT")
    cg.add_global(mqtt_ns.using)

    cg.add(var.set_broker_address(config[CONF_BROKER]))
    cg.add(var.set_broker_port(config[CONF_PORT]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    if CONF_CLIENT_ID in config:
        cg.add(var.set_client_id(config[CONF_CLIENT_ID]))

    discovery = config[CONF_DISCOVERY]
    discovery_retain = config[CONF_DISCOVERY_RETAIN]
    discovery_prefix = config[CONF_DISCOVERY_PREFIX]
    discovery_unique_id_generator = config[CONF_DISCOVERY_UNIQUE_ID_GENERATOR]
    discovery_object_id_generator = config[CONF_DISCOVERY_OBJECT_ID_GENERATOR]

    if not discovery:
        cg.add(var.disable_discovery())
    elif discovery == "CLEAN":
        cg.add(
            var.set_discovery_info(
                discovery_prefix,
                discovery_unique_id_generator,
                discovery_object_id_generator,
                discovery_retain,
                True,
            )
        )
    elif CONF_DISCOVERY_RETAIN in config or CONF_DISCOVERY_PREFIX in config:
        cg.add(
            var.set_discovery_info(
                discovery_prefix,
                discovery_unique_id_generator,
                discovery_object_id_generator,
                discovery_retain,
            )
        )

    cg.add(var.set_topic_prefix(config[CONF_TOPIC_PREFIX]))

    if config[CONF_USE_ABBREVIATIONS]:
        cg.add_define("USE_MQTT_ABBREVIATIONS")

    birth_message = config[CONF_BIRTH_MESSAGE]
    if not birth_message:
        cg.add(var.disable_birth_message())
    else:
        cg.add(var.set_birth_message(exp_mqtt_message(birth_message)))
    will_message = config[CONF_WILL_MESSAGE]
    if not will_message:
        cg.add(var.disable_last_will())
    else:
        cg.add(var.set_last_will(exp_mqtt_message(will_message)))
    shutdown_message = config[CONF_SHUTDOWN_MESSAGE]
    if not shutdown_message:
        cg.add(var.disable_shutdown_message())
    else:
        cg.add(var.set_shutdown_message(exp_mqtt_message(shutdown_message)))

    log_topic = config[CONF_LOG_TOPIC]
    if not log_topic:
        cg.add(var.disable_log_message())
    else:
        cg.add(var.set_log_message_template(exp_mqtt_message(log_topic)))

        if CONF_LEVEL in log_topic:
            cg.add(var.set_log_level(logger.LOG_LEVELS[log_topic[CONF_LEVEL]]))

    if CONF_SSL_FINGERPRINTS in config:
        for fingerprint in config[CONF_SSL_FINGERPRINTS]:
            arr = [
                cg.RawExpression(f"0x{fingerprint[i:i + 2]}") for i in range(0, 40, 2)
            ]
            cg.add(var.add_ssl_fingerprint(arr))
        cg.add_build_flag("-DASYNC_TCP_SSL_ENABLED=1")

    cg.add(var.set_keep_alive(config[CONF_KEEPALIVE]))

    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))

    # esp-idf only
    if CONF_CERTIFICATE_AUTHORITY in config:
        cg.add(var.set_ca_certificate(config[CONF_CERTIFICATE_AUTHORITY]))
        cg.add(var.set_skip_cert_cn_check(config[CONF_SKIP_CERT_CN_CHECK]))

        # prevent error -0x428e
        # See https://github.com/espressif/esp-idf/issues/139
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_HARDWARE_MPI", False)

    if CONF_IDF_SEND_ASYNC in config and config[CONF_IDF_SEND_ASYNC]:
        cg.add_define("USE_MQTT_IDF_ENQUEUE")
    # end esp-idf

    for conf in config.get(CONF_ON_MESSAGE, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], conf[CONF_TOPIC])
        cg.add(trig.set_qos(conf[CONF_QOS]))
        if CONF_PAYLOAD in conf:
            cg.add(trig.set_payload(conf[CONF_PAYLOAD]))
        await cg.register_component(trig, conf)
        await automation.build_automation(trig, [(cg.std_string, "x")], conf)

    for conf in config.get(CONF_ON_JSON_MESSAGE, []):
        trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], conf[CONF_TOPIC], conf[CONF_QOS])
        await automation.build_automation(trig, [(cg.JsonObjectConst, "x")], conf)

    for conf in config.get(CONF_ON_CONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_DISCONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


MQTT_PUBLISH_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(MQTTClientComponent),
        cv.Required(CONF_TOPIC): cv.templatable(cv.publish_topic),
        cv.Required(CONF_PAYLOAD): cv.templatable(cv.mqtt_payload),
        cv.Optional(CONF_QOS, default=0): cv.templatable(cv.mqtt_qos),
        cv.Optional(CONF_RETAIN, default=False): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "mqtt.publish", MQTTPublishAction, MQTT_PUBLISH_ACTION_SCHEMA
)
async def mqtt_publish_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_TOPIC], args, cg.std_string)
    cg.add(var.set_topic(template_))

    template_ = await cg.templatable(config[CONF_PAYLOAD], args, cg.std_string)
    cg.add(var.set_payload(template_))
    template_ = await cg.templatable(config[CONF_QOS], args, cg.uint8)
    cg.add(var.set_qos(template_))
    template_ = await cg.templatable(config[CONF_RETAIN], args, bool)
    cg.add(var.set_retain(template_))
    return var


MQTT_PUBLISH_JSON_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(MQTTClientComponent),
        cv.Required(CONF_TOPIC): cv.templatable(cv.publish_topic),
        cv.Required(CONF_PAYLOAD): cv.lambda_,
        cv.Optional(CONF_QOS, default=0): cv.templatable(cv.mqtt_qos),
        cv.Optional(CONF_RETAIN, default=False): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "mqtt.publish_json", MQTTPublishJsonAction, MQTT_PUBLISH_JSON_ACTION_SCHEMA
)
async def mqtt_publish_json_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_TOPIC], args, cg.std_string)
    cg.add(var.set_topic(template_))

    args_ = args + [(cg.JsonObject, "root")]
    lambda_ = await cg.process_lambda(config[CONF_PAYLOAD], args_, return_type=cg.void)
    cg.add(var.set_payload(lambda_))
    template_ = await cg.templatable(config[CONF_QOS], args, cg.uint8)
    cg.add(var.set_qos(template_))
    template_ = await cg.templatable(config[CONF_RETAIN], args, bool)
    cg.add(var.set_retain(template_))
    return var


def get_default_topic_for(data, component_type, name, suffix):
    allowlist = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"
    sanitized_name = "".join(
        x for x in name.lower().replace(" ", "_") if x in allowlist
    )
    return f"{data.topic_prefix}/{component_type}/{sanitized_name}/{suffix}"


async def register_mqtt_component(var, config):
    await cg.register_component(var, {})

    if CONF_RETAIN in config:
        cg.add(var.set_retain(config[CONF_RETAIN]))
    if not config.get(CONF_DISCOVERY, True):
        cg.add(var.disable_discovery())
    if CONF_STATE_TOPIC in config:
        cg.add(var.set_custom_state_topic(config[CONF_STATE_TOPIC]))
    if CONF_COMMAND_TOPIC in config:
        cg.add(var.set_custom_command_topic(config[CONF_COMMAND_TOPIC]))
    if CONF_COMMAND_RETAIN in config:
        cg.add(var.set_command_retain(config[CONF_COMMAND_RETAIN]))
    if CONF_AVAILABILITY in config:
        availability = config[CONF_AVAILABILITY]
        if not availability:
            cg.add(var.disable_availability())
        else:
            cg.add(
                var.set_availability(
                    availability[CONF_TOPIC],
                    availability[CONF_PAYLOAD_AVAILABLE],
                    availability[CONF_PAYLOAD_NOT_AVAILABLE],
                )
            )


@automation.register_condition(
    "mqtt.connected",
    MQTTConnectedCondition,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(MQTTClientComponent),
        }
    ),
)
async def mqtt_connected_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
