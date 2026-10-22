from dataclasses import dataclass, field

from esphome import automation
import esphome.codegen as cg
from esphome.components import logger, network, socket
from esphome.components.const import CONF_VERIFY_SSL
from esphome.components.esp32 import (
    add_idf_component,
    allow_insecure_tls,
    idf_version,
    include_builtin_idf_component,
    request_tls,
    require_certificate_bundle,
)
from esphome.config_helpers import (
    filter_source_files_from_defines,
    filter_source_files_from_platform,
)
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
    CONF_DISCOVER_IP,
    CONF_DISCOVERY,
    CONF_DISCOVERY_OBJECT_ID_GENERATOR,
    CONF_DISCOVERY_PREFIX,
    CONF_DISCOVERY_RETAIN,
    CONF_DISCOVERY_UNIQUE_ID_GENERATOR,
    CONF_ENABLE_ON_BOOT,
    CONF_ESPHOME,
    CONF_ID,
    CONF_KEEPALIVE,
    CONF_LEVEL,
    CONF_LOG_TOPIC,
    CONF_MQTT,
    CONF_NAME_ADD_MAC_SUFFIX,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_ON_JSON_MESSAGE,
    CONF_ON_MESSAGE,
    CONF_PASSWORD,
    CONF_PAYLOAD,
    CONF_PAYLOAD_AVAILABLE,
    CONF_PAYLOAD_NOT_AVAILABLE,
    CONF_PORT,
    CONF_PUBLISH_NAN_AS_NONE,
    CONF_QOS,
    CONF_REBOOT_TIMEOUT,
    CONF_RETAIN,
    CONF_SHUTDOWN_MESSAGE,
    CONF_SKIP_CERT_CN_CHECK,
    CONF_STATE_TOPIC,
    CONF_SUBSCRIBE_QOS,
    CONF_TOPIC,
    CONF_TOPIC_PREFIX,
    CONF_USE_ABBREVIATIONS,
    CONF_USERNAME,
    CONF_WILL_MESSAGE,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RTL87XX,
    Framework,
    PlatformFramework,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

DEPENDENCIES = ["network"]
DOMAIN = "mqtt"


def AUTO_LOAD():
    if CORE.is_esp8266 or CORE.is_libretiny:
        return ["async_tcp", "json"]
    return ["json"]


CONF_TLS = "tls"
CONF_IDF_SEND_ASYNC = "idf_send_async"
CONF_WAIT_FOR_CONNECTION = "wait_for_connection"
CONF_TRANSPORT = "transport"
CONF_WS_PATH = "ws_path"

MQTT_TRANSPORT_TCP = "tcp"
MQTT_TRANSPORT_WS = "ws"
MQTT_TRANSPORT_WSS = "wss"
DEFAULT_WS_PATH = "/mqtt"

# Max lengths for stack-based topic building.
# These values are used in cv.Length() validators below to ensure the C++ code
# in mqtt_component.cpp can safely use fixed-size stack buffers without overflow.
# If you change these, update the corresponding constants in mqtt_component.cpp.
TOPIC_PREFIX_MAX_LEN = 64  # Default is device name, typically short
DISCOVERY_PREFIX_MAX_LEN = 64  # Default is "homeassistant" (13 chars)

DOMAIN = "mqtt"


@dataclass
class MQTTData:
    """Validation state that code generation needs again."""

    # Message config key -> topic suffix, for topics synthesised from the topic prefix
    default_topics: dict[str, str] = field(default_factory=dict)


def _get_data() -> MQTTData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = MQTTData()
    return CORE.data[DOMAIN]


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
MQTTClientDisconnectReason = mqtt_ns.enum("MQTTClientDisconnectReason")
MQTTClientComponent = mqtt_ns.class_("MQTTClientComponent", cg.Component)
MQTTPublishJsonAction = mqtt_ns.class_("MQTTPublishJsonAction", automation.Action)
MQTTComponent = mqtt_ns.class_("MQTTComponent", cg.Component)

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

# Only ESP-IDF's esp-mqtt implements MQTT over WebSocket
MQTTTransport = mqtt_ns.enum("MQTTTransport", is_class=True)
MQTT_TRANSPORT_OPTIONS = {
    MQTT_TRANSPORT_TCP: MQTTTransport.MQTT_TRANSPORT_TCP,
    MQTT_TRANSPORT_WS: MQTTTransport.MQTT_TRANSPORT_WS,
    MQTT_TRANSPORT_WSS: MQTTTransport.MQTT_TRANSPORT_WSS,
}


def _validate_transport(value: str) -> str:
    value = cv.enum(MQTT_TRANSPORT_OPTIONS, lower=True)(value)
    if value in (MQTT_TRANSPORT_WS, MQTT_TRANSPORT_WSS):
        if not CORE.is_esp32:
            raise cv.Invalid(
                f"transport '{value}' is currently only supported on ESP32"
            )
        if CORE.target_framework != Framework.ESP_IDF:
            raise cv.Invalid(f"transport '{value}' requires the esp-idf framework")
    return value


# (config key, topic suffix, payload) for messages defaulted from the topic prefix
_DEFAULT_MESSAGES: tuple[tuple[str, str, str | None], ...] = (
    (CONF_BIRTH_MESSAGE, "/status", "online"),
    (CONF_WILL_MESSAGE, "/status", "offline"),
    (CONF_SHUTDOWN_MESSAGE, "/status", "offline"),
    (CONF_LOG_TOPIC, "/debug", None),
)


def validate_config(value):
    if (
        value.get(CONF_TRANSPORT) not in (MQTT_TRANSPORT_WS, MQTT_TRANSPORT_WSS)
        and CONF_WS_PATH in value
    ):
        raise cv.Invalid(
            f"'{CONF_WS_PATH}' is only valid when 'transport' is 'ws' or 'wss'"
        )

    # Populate default fields
    out = value.copy()
    topic_prefix = value[CONF_TOPIC_PREFIX]
    default_topics = _get_data().default_topics
    # If the topic prefix is not null and these messages are not configured, then set them to the default
    # If the topic prefix is null and these messages are not configured, then set them to null
    for key, suffix, payload in _DEFAULT_MESSAGES:
        if key in value:
            continue
        if topic_prefix == "":
            out[key] = {}
            continue
        message = {CONF_TOPIC: f"{topic_prefix}{suffix}"}
        if payload is not None:
            message[CONF_PAYLOAD] = payload
        message[CONF_QOS] = 0
        message[CONF_RETAIN] = True
        out[key] = message
        default_topics[key] = suffix
    return out


# Keys that only make sense over TLS; any of them switches it on
TLS_IMPLYING_KEYS = frozenset(
    {CONF_CERTIFICATE_AUTHORITY, CONF_VERIFY_SSL, CONF_CLIENT_CERTIFICATE}
)


def _derive_tls(config: ConfigType) -> ConfigType:
    """Derive ``tls`` from the certificate options and the transport, reject contradictions, pick the port."""
    # skip_cert_cn_check has a default on ESP32, so its value decides, not its presence
    if config.get(CONF_VERIFY_SSL) is False and config.get(CONF_SKIP_CERT_CN_CHECK):
        raise cv.Invalid(
            "'skip_cert_cn_check' has no effect when 'verify_ssl' is false."
        )
    transport = config.get(CONF_TRANSPORT)
    if (
        config.keys() & TLS_IMPLYING_KEYS
        or config.get(CONF_SKIP_CERT_CN_CHECK)
        or transport == MQTT_TRANSPORT_WSS
    ):
        if config.get(CONF_TLS) is False:
            raise cv.Invalid(
                "'tls' cannot be false when a certificate, 'verify_ssl', "
                "'skip_cert_cn_check' or transport 'wss' is set."
            )
        config[CONF_TLS] = True
    # TLS with plain ws is almost certainly a typo for wss
    if transport == MQTT_TRANSPORT_WS and config.get(CONF_TLS):
        raise cv.Invalid(
            "transport 'ws' is unencrypted; remove the TLS options "
            "or switch to transport 'wss'"
        )
    config.setdefault(CONF_PORT, 8883 if config.get(CONF_TLS) else 1883)
    return config


_validate_tls = cv.All(
    cv.only_when(CONF_CERTIFICATE_AUTHORITY, CONF_VERIFY_SSL, False, negate=True),
    _derive_tls,
)


def _consume_mqtt_sockets(config: ConfigType) -> ConfigType:
    """Register socket needs for MQTT component."""
    # MQTT needs 1 socket for the broker connection
    socket.consume_sockets(1, "mqtt")(config)
    return config


# The parsed JSON document is freed once the automation returns
JSON_MESSAGE_AUTOMATION_SCHEMA = automation.validate_synchronous_automation(
    "on_json_message",
    "the parsed JSON document",
    {
        cv.Required(CONF_TOPIC): cv.subscribe_topic,
        cv.Optional(CONF_QOS, default=0): cv.mqtt_qos,
    },
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MQTTClientComponent),
            cv.Required(CONF_BROKER): cv.string_strict,
            cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_PORT): cv.port,
            cv.Optional(CONF_USERNAME, default=""): cv.string,
            cv.Optional(CONF_PASSWORD, default=""): cv.sensitive(),
            cv.Optional(CONF_CLEAN_SESSION, default=False): cv.boolean,
            cv.Optional(CONF_CLIENT_ID): cv.string,
            cv.Optional(
                CONF_TRANSPORT, default=MQTT_TRANSPORT_TCP
            ): _validate_transport,
            cv.Optional(CONF_WS_PATH): cv.string,
            cv.SplitDefault(CONF_IDF_SEND_ASYNC, esp32=False): cv.All(
                cv.boolean, cv.only_on_esp32
            ),
            cv.Optional(CONF_TLS): cv.All(cv.boolean, cv.only_on_esp32),
            cv.Optional(CONF_VERIFY_SSL): cv.All(cv.boolean, cv.only_on_esp32),
            cv.Optional(CONF_CERTIFICATE_AUTHORITY): cv.All(
                cv.string, cv.only_on_esp32
            ),
            cv.Inclusive(CONF_CLIENT_CERTIFICATE, "cert-key-pair"): cv.All(
                cv.string, cv.only_on_esp32
            ),
            cv.Inclusive(CONF_CLIENT_CERTIFICATE_KEY, "cert-key-pair"): cv.sensitive(
                cv.All(cv.string, cv.only_on_esp32)
            ),
            cv.SplitDefault(CONF_SKIP_CERT_CN_CHECK, esp32=False): cv.All(
                cv.boolean, cv.only_on_esp32
            ),
            cv.Optional(CONF_DISCOVERY, default=True): cv.Any(
                cv.boolean, cv.one_of("CLEAN", upper=True)
            ),
            cv.Optional(CONF_DISCOVERY_RETAIN, default=True): cv.boolean,
            cv.Optional(CONF_DISCOVER_IP, default=True): cv.boolean,
            cv.Optional(CONF_DISCOVERY_PREFIX, default="homeassistant"): cv.All(
                cv.publish_topic, cv.Length(max=DISCOVERY_PREFIX_MAX_LEN)
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
            cv.Optional(CONF_TOPIC_PREFIX, default=lambda: CORE.name): cv.All(
                cv.publish_topic, cv.Length(max=TOPIC_PREFIX_MAX_LEN)
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
            cv.Optional(CONF_KEEPALIVE, default="15s"): cv.positive_time_period_seconds,
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="15min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(),
            cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
                {
                    cv.Required(CONF_TOPIC): cv.subscribe_topic,
                    cv.Optional(CONF_QOS, default=0): cv.mqtt_qos,
                    cv.Optional(CONF_PAYLOAD): cv.string_strict,
                }
            ),
            cv.Optional(CONF_ON_JSON_MESSAGE): JSON_MESSAGE_AUTOMATION_SCHEMA,
            cv.Optional(CONF_PUBLISH_NAN_AS_NONE, default=False): cv.boolean,
            cv.Optional(CONF_WAIT_FOR_CONNECTION, default=False): cv.boolean,
        }
    ),
    validate_config,
    _validate_tls,
    cv.only_on(
        [
            PLATFORM_BK72XX,
            PLATFORM_ESP32,
            PLATFORM_ESP8266,
            PLATFORM_LN882X,
            PLATFORM_RTL87XX,
        ]
    ),
    _consume_mqtt_sockets,
    network.require_ipv4(CONF_MQTT),
)


def exp_mqtt_message(config, topic: cg.MockObj | str | None = None):
    if config is None:
        return cg.optional(cg.TemplateArguments(MQTTMessage))
    if topic is None:
        topic = config[CONF_TOPIC]
    return cg.StructInitializer(
        MQTTMessage,
        ("topic", topic),
        ("payload", config.get(CONF_PAYLOAD, "")),
        ("qos", config[CONF_QOS]),
        ("retain", config[CONF_RETAIN]),
    )


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_CONNECT, "set_on_connect", [(cg.bool_, "session_present")]
    ),
    automation.CallbackAutomation(
        CONF_ON_DISCONNECT,
        "set_on_disconnect",
        [(MQTTClientDisconnectReason, "reason")],
    ),
)


@coroutine_with_priority(CoroPriority.WEB)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Add required libraries for ESP8266 and LibreTiny
    if CORE.is_esp8266 or CORE.is_libretiny:
        # https://github.com/heman/async-mqtt-client/blob/master/library.json
        cg.add_library("heman/AsyncMqttClient-esphome", "2.0.0")

    if CORE.is_esp32:
        # Re-enable ESP-IDF's mqtt component (excluded by default to save compile time)
        # IDF 6.0 moved esp-mqtt to an external component
        if idf_version() >= cv.Version(6, 0, 0):
            add_idf_component(name="espressif/mqtt", ref="1.0.0")
        else:
            include_builtin_idf_component("mqtt")
        # esp-mqtt links transport_ssl.c (esp_tls) even for plain MQTT
        request_tls()

    cg.add_define("USE_MQTT")
    cg.add_global(mqtt_ns.using)

    cg.add(var.set_broker_address(config[CONF_BROKER]))
    cg.add(var.set_enable_on_boot(config[CONF_ENABLE_ON_BOOT]))
    cg.add(var.set_broker_port(config[CONF_PORT]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_clean_session(config[CONF_CLEAN_SESSION]))
    cg.add_optional(var.set_client_id, config.get(CONF_CLIENT_ID))

    discovery = config[CONF_DISCOVERY]
    discovery_retain = config[CONF_DISCOVERY_RETAIN]
    discovery_prefix = config[CONF_DISCOVERY_PREFIX]
    discovery_unique_id_generator = config[CONF_DISCOVERY_UNIQUE_ID_GENERATOR]
    discovery_object_id_generator = config[CONF_DISCOVERY_OBJECT_ID_GENERATOR]
    discover_ip = config[CONF_DISCOVER_IP]

    if not discovery:
        discovery_prefix = ""

    if not discovery and not discover_ip:
        cg.add(var.disable_discovery())
    elif discovery == "CLEAN":
        cg.add(
            var.set_discovery_info(
                discovery_prefix,
                discovery_unique_id_generator,
                discovery_object_id_generator,
                discovery_retain,
                discover_ip,
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
                discover_ip,
            )
        )

    topic_prefix: cg.MockObj | str = config[CONF_TOPIC_PREFIX]
    # The MAC suffix is only known at runtime, so a prefix defaulted to the device name
    # is taken from App.get_name() rather than baked in as a literal
    if (
        topic_prefix == CORE.name
        and CORE.config[CONF_ESPHOME][CONF_NAME_ADD_MAC_SUFFIX]
    ):
        topic_prefix = cg.App.get_name()
    cg.add(var.set_topic_prefix(topic_prefix))

    default_topics = _get_data().default_topics

    def message(key: str) -> cg.StructInitializer:
        # Topics synthesised from the prefix follow the prefix expression; explicit topics stay literal
        suffix = default_topics.get(key)
        topic = None if suffix is None else topic_prefix + suffix
        return exp_mqtt_message(config[key], topic)

    if config[CONF_USE_ABBREVIATIONS]:
        cg.add_define("USE_MQTT_ABBREVIATIONS")

    birth_message = config[CONF_BIRTH_MESSAGE]
    if not birth_message:
        cg.add(var.disable_birth_message())
    else:
        cg.add(var.set_birth_message(message(CONF_BIRTH_MESSAGE)))
    will_message = config[CONF_WILL_MESSAGE]
    if not will_message:
        cg.add(var.disable_last_will())
    else:
        cg.add(var.set_last_will(message(CONF_WILL_MESSAGE)))
    shutdown_message = config[CONF_SHUTDOWN_MESSAGE]
    if not shutdown_message:
        cg.add(var.disable_shutdown_message())
    else:
        cg.add(var.set_shutdown_message(message(CONF_SHUTDOWN_MESSAGE)))

    log_topic = config[CONF_LOG_TOPIC]
    if not log_topic:
        cg.add(var.disable_log_message())
    else:
        cg.add(var.set_log_message_template(message(CONF_LOG_TOPIC)))
        # Request a log listener slot only when log topic is enabled
        logger.request_log_listener()

        if CONF_LEVEL in log_topic:
            cg.add(var.set_log_level(logger.LOG_LEVELS[log_topic[CONF_LEVEL]]))

    cg.add(var.set_keep_alive(config[CONF_KEEPALIVE]))

    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))

    # esp-idf only
    transport = config.get(CONF_TRANSPORT)
    if config.get(CONF_TLS):
        cg.add_define("USE_MQTT_TLS")
        if (client_certificate := config.get(CONF_CLIENT_CERTIFICATE)) is not None:
            cg.add(var.set_cl_certificate(client_certificate))
            cg.add(var.set_cl_key(config[CONF_CLIENT_CERTIFICATE_KEY]))
        if config.get(CONF_SKIP_CERT_CN_CHECK):
            cg.add(var.set_skip_cert_cn_check(True))
        # Trust source: none, the given CA, or the certificate bundle
        if config.get(CONF_VERIFY_SSL) is False:
            cg.add_define("USE_MQTT_TLS_INSECURE")
            allow_insecure_tls()
        elif (ca := config.get(CONF_CERTIFICATE_AUTHORITY)) is not None:
            cg.add(var.set_ca_certificate(ca))
        else:
            cg.add_define("USE_MQTT_CERT_BUNDLE")
            require_certificate_bundle()

    if CORE.is_esp32:
        if transport != MQTT_TRANSPORT_TCP:
            cg.add(var.set_transport(transport))
        if transport in (MQTT_TRANSPORT_WS, MQTT_TRANSPORT_WSS):
            cg.add(var.set_ws_path(config.get(CONF_WS_PATH, DEFAULT_WS_PATH)))

    if CONF_IDF_SEND_ASYNC in config and config[CONF_IDF_SEND_ASYNC]:
        cg.add_define("USE_MQTT_IDF_ENQUEUE")
    # end esp-idf

    # The client queues subscriptions until it connects, so they can be made at construction.
    for conf in config.get(CONF_ON_MESSAGE, []):
        callback = await automation.build_trigger_callback(
            [(cg.std_string, "x")],
            conf,
            params=[(cg.std_string, "topic"), (cg.std_string, "payload")],
            forward=["payload"],
            # The length is compared first; the literal stays in flash on ESP8266.
            when=automation.ApplyCall(
                "StringRef(payload) == {}",
                ((CONF_PAYLOAD, cg.std_string, automation.string_ref_literal),),
            ),
        )
        cg.add(
            var.subscribe(cg.progmem_string(conf[CONF_TOPIC]), callback, conf[CONF_QOS])
        )

    for conf in config.get(CONF_ON_JSON_MESSAGE, []):
        callback = await automation.build_trigger_callback(
            [(cg.JsonObjectConst, "x")],
            conf,
            params=[(cg.std_string, "topic"), (cg.JsonObject, "root")],
            forward=["root"],
        )
        cg.add(
            var.subscribe_json(
                cg.progmem_string(conf[CONF_TOPIC]), callback, conf[CONF_QOS]
            )
        )

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)

    cg.add(var.set_publish_nan_as_none(config[CONF_PUBLISH_NAN_AS_NONE]))

    cg.add(var.set_wait_for_connection(config[CONF_WAIT_FOR_CONNECTION]))


MQTT_PUBLISH_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(MQTTClientComponent),
        cv.Required(CONF_TOPIC): cv.templatable(cv.publish_topic),
        cv.Required(CONF_PAYLOAD): cv.templatable(cv.mqtt_payload),
        cv.Optional(CONF_QOS, default=0): cv.templatable(cv.mqtt_qos),
        cv.Optional(CONF_RETAIN, default=False): cv.templatable(cv.boolean),
    }
)


# A bare literal is ambiguous between the std::string and (const char *, size_t) publish
# overloads, so constants and inlined `return "...";` lambdas are both spelled as std::string.
def _std_string(config: ConfigType, value: str) -> str:
    rendered = automation.flash_string(config, value)
    return rendered if CORE.is_esp8266 else f"std::string({rendered})"


automation.register_apply_action(
    "mqtt.publish",
    MQTT_PUBLISH_ACTION_SCHEMA,
    automation.ApplyCall(
        "publish({}, {}, {}, {})",
        (
            (CONF_TOPIC, "std::string", _std_string),
            (CONF_PAYLOAD, "std::string", _std_string),
            (CONF_QOS, cg.uint8),
            (CONF_RETAIN, cg.bool_),
        ),
    ),
)


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
    "mqtt.publish_json",
    MQTTPublishJsonAction,
    MQTT_PUBLISH_JSON_ACTION_SCHEMA,
    synchronous=True,
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
    template_ = await cg.templatable(config[CONF_RETAIN], args, cg.bool_)
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

    cg.add_optional(var.set_qos, config.get(CONF_QOS))
    cg.add_optional(var.set_retain, config.get(CONF_RETAIN))
    cg.add_optional(var.set_subscribe_qos, config.get(CONF_SUBSCRIBE_QOS))
    if not config.get(CONF_DISCOVERY, True):
        cg.add(var.disable_discovery())
    if CONF_STATE_TOPIC in config:
        state_topic = await cg.templatable(config[CONF_STATE_TOPIC], [], cg.std_string)
        cg.add(var.set_custom_state_topic(state_topic))
    if CONF_COMMAND_TOPIC in config:
        command_topic = await cg.templatable(
            config[CONF_COMMAND_TOPIC], [], cg.std_string
        )
        cg.add(var.set_custom_command_topic(command_topic))
    cg.add_optional(var.set_command_retain, config.get(CONF_COMMAND_RETAIN))
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


automation.register_apply_condition(
    "mqtt.connected",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(MQTTClientComponent),
        }
    ),
    "is_connected()",
)


for _name, _call in (("mqtt.enable", "enable()"), ("mqtt.disable", "disable()")):
    automation.register_apply_action(
        _name,
        cv.Schema(
            {
                cv.GenerateID(): cv.use_id(MQTTClientComponent),
            }
        ),
        automation.ApplyCall(_call),
    )


_platform_filter = filter_source_files_from_platform(
    {
        "mqtt_backend_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
    }
)

# Each entity file is fully #ifdef'd on the USE_<ENTITY> define the core
# emits for entity platforms present in the config.
_define_filter = filter_source_files_from_defines(
    {
        "mqtt_alarm_control_panel.cpp": "USE_ALARM_CONTROL_PANEL",
        "mqtt_binary_sensor.cpp": "USE_BINARY_SENSOR",
        "mqtt_button.cpp": "USE_BUTTON",
        "mqtt_climate.cpp": "USE_CLIMATE",
        "mqtt_cover.cpp": "USE_COVER",
        "mqtt_date.cpp": "USE_DATETIME_DATE",
        "mqtt_datetime.cpp": "USE_DATETIME_DATETIME",
        "mqtt_event.cpp": "USE_EVENT",
        "mqtt_fan.cpp": "USE_FAN",
        "mqtt_light.cpp": "USE_LIGHT",
        "mqtt_lock.cpp": "USE_LOCK",
        "mqtt_number.cpp": "USE_NUMBER",
        "mqtt_select.cpp": "USE_SELECT",
        "mqtt_sensor.cpp": "USE_SENSOR",
        "mqtt_switch.cpp": "USE_SWITCH",
        "mqtt_text.cpp": "USE_TEXT",
        "mqtt_text_sensor.cpp": "USE_TEXT_SENSOR",
        "mqtt_time.cpp": "USE_DATETIME_TIME",
        "mqtt_update.cpp": "USE_UPDATE",
        "mqtt_valve.cpp": "USE_VALVE",
    }
)


def FILTER_SOURCE_FILES() -> list[str]:
    return _platform_filter() + _define_filter()
