import re

from esphome import automation
from esphome.automation import Condition
import esphome.codegen as cg
from esphome.components import logger
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import (
    CONF_AVAILABILITY,
    CONF_BIRTH_MESSAGE,
    CONF_BROKER,
    CONF_CERTIFICATE_AUTHORITY,
    CONF_CLEAN_SESSION,
    CONF_CLIENT_CERTIFICATE,
    CONF_CLIENT_CERTIFICATE_KEY,
    CONF_CLIENT_ID,
    CONF_COMMAND_RETAIN,
    CONF_COMMAND_TOPIC,
    CONF_DISCOVERY,
    CONF_DISCOVERY_OBJECT_ID_GENERATOR,
    CONF_DISCOVERY_PREFIX,
    CONF_DISCOVERY_RETAIN,
    CONF_DISCOVERY_UNIQUE_ID_GENERATOR,
    CONF_ID,
    CONF_KEEPALIVE,
    CONF_LEVEL,
    CONF_LOG_TOPIC,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_ON_JSON_MESSAGE,
    CONF_ON_MESSAGE,
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
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE, coroutine_with_priority

DEPENDENCIES = ["network"]


def AUTO_LOAD():
    if CORE.is_esp8266 or CORE.is_libretiny:
        return ["async_tcp", "json"]
    return ["json"]


CONF_DISCOVER_IP = "discover_ip"
CONF_IDF_SEND_ASYNC = "idf_send_async"
CONF_SKIP_CERT_CN_CHECK = "skip_cert_cn_check"
CONF_SET_DISCOVERY_INFO_ACTION_ID_ = "set_discovery_info_action_id_"


def validate_message_just_topic(value):
    value = cv.publish_topic(value)
    return MQTT_MESSAGE_BASE({CONF_TOPIC: value})


MQTT_MESSAGE_BASE = cv.Schema(
    {
        cv.Required(CONF_TOPIC): cv.templatable(cv.publish_topic),
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
MQTTSetDiscoveryAction = mqtt_ns.class_("MQTTSetDiscoveryAction", automation.Action)
MQTTSetDiscoveryInfoAction = mqtt_ns.class_(
    "MQTTSetDiscoveryInfoAction", automation.Action
)
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

MQTTAlarmControlPanelComponent = mqtt_ns.class_(
    "MQTTAlarmControlPanelComponent", MQTTComponent
)
MQTTBinarySensorComponent = mqtt_ns.class_("MQTTBinarySensorComponent", MQTTComponent)
MQTTClimateComponent = mqtt_ns.class_("MQTTClimateComponent", MQTTComponent)
MQTTCoverComponent = mqtt_ns.class_("MQTTCoverComponent", MQTTComponent)
MQTTFanComponent = mqtt_ns.class_("MQTTFanComponent", MQTTComponent)
MQTTJSONLightComponent = mqtt_ns.class_("MQTTJSONLightComponent", MQTTComponent)
MQTTSensorComponent = mqtt_ns.class_("MQTTSensorComponent", MQTTComponent)
MQTTSwitchComponent = mqtt_ns.class_("MQTTSwitchComponent", MQTTComponent)
MQTTTextSensor = mqtt_ns.class_("MQTTTextSensor", MQTTComponent)
MQTTNumberComponent = mqtt_ns.class_("MQTTNumberComponent", MQTTComponent)
MQTTDateComponent = mqtt_ns.class_("MQTTDateComponent", MQTTComponent)
MQTTTimeComponent = mqtt_ns.class_("MQTTTimeComponent", MQTTComponent)
MQTTDateTimeComponent = mqtt_ns.class_("MQTTDateTimeComponent", MQTTComponent)
MQTTTextComponent = mqtt_ns.class_("MQTTTextComponent", MQTTComponent)
MQTTSelectComponent = mqtt_ns.class_("MQTTSelectComponent", MQTTComponent)
MQTTButtonComponent = mqtt_ns.class_("MQTTButtonComponent", MQTTComponent)
MQTTLockComponent = mqtt_ns.class_("MQTTLockComponent", MQTTComponent)
MQTTEventComponent = mqtt_ns.class_("MQTTEventComponent", MQTTComponent)
MQTTUpdateComponent = mqtt_ns.class_("MQTTUpdateComponent", MQTTComponent)
MQTTValveComponent = mqtt_ns.class_("MQTTValveComponent", MQTTComponent)

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


def validate_fingerprint(value):
    value = cv.string(value)
    if re.match(r"^[0-9a-f]{40}$", value) is None:
        raise cv.Invalid("fingerprint must be valid SHA1 hash")
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MQTTClientComponent),
            cv.Required(CONF_BROKER): cv.templatable(cv.string_strict),
            cv.Optional(CONF_PORT, default=1883): cv.templatable(cv.port),
            cv.Optional(CONF_USERNAME, default=""): cv.templatable(cv.string),
            cv.Optional(CONF_PASSWORD, default=""): cv.templatable(cv.string),
            cv.Optional(CONF_CLEAN_SESSION, default=False): cv.boolean,
            cv.Optional(CONF_CLIENT_ID): cv.templatable(cv.string),
            cv.SplitDefault(CONF_IDF_SEND_ASYNC, esp32_idf=False): cv.All(
                cv.boolean, cv.only_with_esp_idf
            ),
            cv.Optional(CONF_CERTIFICATE_AUTHORITY): cv.All(
                cv.string, cv.only_with_esp_idf
            ),
            cv.Inclusive(CONF_CLIENT_CERTIFICATE, "cert-key-pair"): cv.All(
                cv.string, cv.only_on_esp32
            ),
            cv.Inclusive(CONF_CLIENT_CERTIFICATE_KEY, "cert-key-pair"): cv.All(
                cv.string, cv.only_on_esp32
            ),
            cv.SplitDefault(CONF_SKIP_CERT_CN_CHECK, esp32_idf=False): cv.All(
                cv.boolean, cv.only_with_esp_idf
            ),
            cv.GenerateID(CONF_SET_DISCOVERY_INFO_ACTION_ID_): cv.declare_id(
                MQTTSetDiscoveryInfoAction
            ),
            cv.Optional(CONF_DISCOVERY, default=True): cv.Any(
                cv.templatable(cv.boolean), cv.one_of("CLEAN", upper=True)
            ),
            cv.Optional(CONF_DISCOVERY_RETAIN, default=True): cv.templatable(
                cv.boolean
            ),
            cv.Optional(CONF_DISCOVER_IP, default=True): cv.templatable(cv.boolean),
            cv.Optional(CONF_DISCOVERY_PREFIX, default="homeassistant"): cv.templatable(
                cv.publish_topic
            ),
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
            cv.Optional(CONF_TOPIC_PREFIX, default=lambda: CORE.name): cv.templatable(
                cv.publish_topic
            ),
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
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_BK72XX]),
)


async def instant_templatable(value, args, output_type, to_exp=None):
    """Generate code for a templatable config option.

    If `value` is a templated value, the lambda expression is returned with
    inline exectution with the provided arguments.
    Otherwise the value is returned as-is (optionally process with to_exp).

    :param value: The value to process.
    :param args: The arguments for the lambda expression.
    :param output_type: The output type of the lambda expression.
    :param to_exp: An optional callable to use for converting non-templated values.
    :return: The potentially templated value.
    """
    if cg.is_template(value):
        lambda_code = await cg.process_lambda(value, args, return_type=output_type)
        return cg.RawExpression(f'({lambda_code})({", ".join(args)})')
    if to_exp is None:
        return value
    if isinstance(to_exp, dict):
        return to_exp[value]
    return to_exp(value)


async def exp_mqtt_message(config):
    if config is None:
        return cg.optional(cg.TemplateArguments(MQTTMessage))
    exp = cg.StructInitializer(
        MQTTMessage,
        ("topic", await instant_templatable(config[CONF_TOPIC], [], cg.std_string)),
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

    cg.add(
        var.set_broker_address(
            await instant_templatable(config[CONF_BROKER], [], cg.std_string)
        )
    )
    cg.add(
        var.set_broker_port(await instant_templatable(config[CONF_PORT], [], cg.uint16))
    )
    cg.add(
        var.set_username(
            await instant_templatable(config[CONF_USERNAME], [], cg.std_string)
        )
    )
    cg.add(
        var.set_password(
            await instant_templatable(config[CONF_PASSWORD], [], cg.std_string)
        )
    )
    cg.add(var.set_clean_session(config[CONF_CLEAN_SESSION]))
    if CONF_CLIENT_ID in config:
        cg.add(
            var.set_client_id(
                await instant_templatable(config[CONF_CLIENT_ID], [], cg.std_string)
            )
        )

    set_discovery_info_action = cg.new_Pvariable(
        config[CONF_SET_DISCOVERY_INFO_ACTION_ID_], cg.TemplateArguments(None), var
    )
    if config[CONF_DISCOVERY] == "CLEAN":
        cg.add(set_discovery_info_action.set_enable(True))
        cg.add(set_discovery_info_action.set_clean(True))
    else:
        cg.add(
            set_discovery_info_action.set_enable(
                await cg.templatable(config[CONF_DISCOVERY], [], bool)
            )
        )
        cg.add(set_discovery_info_action.set_clean(False))
    cg.add(
        set_discovery_info_action.set_prefix(
            await cg.templatable(config[CONF_DISCOVERY_PREFIX], [], cg.std_string)
        )
    )
    cg.add(
        set_discovery_info_action.set_retain(
            await cg.templatable(config[CONF_DISCOVERY_RETAIN], [], bool)
        )
    )
    cg.add(
        set_discovery_info_action.set_unique_id_generator(
            config[CONF_DISCOVERY_UNIQUE_ID_GENERATOR]
        )
    )
    cg.add(
        set_discovery_info_action.set_discover_ip(
            await cg.templatable(config[CONF_DISCOVER_IP], [], bool)
        )
    )
    cg.add(
        set_discovery_info_action.set_object_id_generator(
            config[CONF_DISCOVERY_OBJECT_ID_GENERATOR]
        )
    )
    cg.add(set_discovery_info_action.play())

    topic_prefix = await instant_templatable(
        config[CONF_TOPIC_PREFIX], [], cg.std_string
    )
    cg.add(var.set_topic_prefix(topic_prefix))

    if config[CONF_USE_ABBREVIATIONS]:
        cg.add_define("USE_MQTT_ABBREVIATIONS")

    if CONF_BIRTH_MESSAGE not in config:
        if topic_prefix == "":
            cg.add(var.disable_birth_message())
        else:
            birth_topic = (
                f"{topic_prefix}/status"
                if not cg.is_template(config[CONF_TOPIC_PREFIX])
                else cg.RawExpression(f'{topic_prefix}+"/status"')
            )
            birth_message_exp = cg.StructInitializer(
                MQTTMessage,
                ("topic", birth_topic),
                ("payload", "online"),
                ("qos", 0),
                ("retain", True),
            )
            cg.add(var.set_birth_message(birth_message_exp))
    else:
        birth_message = config[CONF_BIRTH_MESSAGE]
        if not birth_message:
            cg.add(var.disable_birth_message())
        else:
            cg.add(var.set_birth_message(await exp_mqtt_message(birth_message)))

    if CONF_WILL_MESSAGE not in config:
        if topic_prefix == "":
            cg.add(var.disable_last_will())
        else:
            will_message_topic = (
                f"{topic_prefix}/status"
                if not cg.is_template(config[CONF_TOPIC_PREFIX])
                else cg.RawExpression(f'{topic_prefix}+"/status"')
            )
            will_message_exp = cg.StructInitializer(
                MQTTMessage,
                ("topic", will_message_topic),
                ("payload", "offline"),
                ("qos", 0),
                ("retain", True),
            )
            cg.add(var.set_last_will(will_message_exp))
    else:
        will_message = config[CONF_WILL_MESSAGE]
        if not will_message:
            cg.add(var.disable_last_will())
        else:
            cg.add(var.set_last_will(await exp_mqtt_message(will_message)))

    if CONF_SHUTDOWN_MESSAGE not in config:
        if topic_prefix == "":
            cg.add(var.disable_shutdown_message())
        else:
            shutdown_message_topic = (
                f"{topic_prefix}/status"
                if not cg.is_template(config[CONF_TOPIC_PREFIX])
                else cg.RawExpression(f'{topic_prefix}+"/status"')
            )
            shutdown_message_exp = cg.StructInitializer(
                MQTTMessage,
                ("topic", shutdown_message_topic),
                ("payload", "offline"),
                ("qos", 0),
                ("retain", True),
            )
            cg.add(var.set_shutdown_message(shutdown_message_exp))
    else:
        shutdown_message = config[CONF_SHUTDOWN_MESSAGE]
        if not shutdown_message:
            cg.add(var.disable_shutdown_message())
        else:
            cg.add(var.set_shutdown_message(await exp_mqtt_message(shutdown_message)))

    if CONF_LOG_TOPIC not in config:
        if topic_prefix == "":
            cg.add(var.disable_log_message())
        else:
            log_message_topic = (
                f"{topic_prefix}/debug"
                if not cg.is_template(config[CONF_TOPIC_PREFIX])
                else cg.RawExpression(f'{topic_prefix}+"/debug"')
            )
            log_message_exp = cg.StructInitializer(
                MQTTMessage,
                ("topic", log_message_topic),
                ("payload", ""),
                ("qos", 0),
                ("retain", True),
            )
            cg.add(var.set_log_message_template(log_message_exp))
    else:
        log_topic = config[CONF_LOG_TOPIC]
        if not log_topic:
            cg.add(var.disable_log_message())
        else:
            cg.add(var.set_log_message_template(await exp_mqtt_message(log_topic)))
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
        if CONF_CLIENT_CERTIFICATE in config:
            cg.add(var.set_cl_certificate(config[CONF_CLIENT_CERTIFICATE]))
            cg.add(var.set_cl_key(config[CONF_CLIENT_CERTIFICATE_KEY]))

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

    if CONF_QOS in config:
        cg.add(var.set_qos(config[CONF_QOS]))
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
