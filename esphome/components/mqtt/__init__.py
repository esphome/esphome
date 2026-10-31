from collections.abc import Iterator

from esphome import automation, preferences
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
from esphome.config import iter_ids
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
    CONF_DEVICE_ID,
    CONF_DEVICES,
    CONF_DISCOVER_IP,
    CONF_DISCOVERY,
    CONF_DISCOVERY_OBJECT_ID_GENERATOR,
    CONF_DISCOVERY_PREFIX,
    CONF_DISCOVERY_RETAIN,
    CONF_DISCOVERY_UNIQUE_ID_GENERATOR,
    CONF_ENABLE_ON_BOOT,
    CONF_ESPHOME,
    CONF_ID,
    CONF_INTERNAL,
    CONF_KEEPALIVE,
    CONF_LEVEL,
    CONF_LOG_TOPIC,
    CONF_MODE_COMMAND_TOPIC,
    CONF_MQTT,
    CONF_MQTT_ID,
    CONF_NAME,
    CONF_NAME_ADD_MAC_SUFFIX,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_ON_JSON_MESSAGE,
    CONF_ON_MESSAGE,
    CONF_PASSWORD,
    CONF_PAYLOAD,
    CONF_PAYLOAD_AVAILABLE,
    CONF_PAYLOAD_NOT_AVAILABLE,
    CONF_PLATFORM,
    CONF_PORT,
    CONF_PUBLISH_NAN_AS_NONE,
    CONF_QOS,
    CONF_REBOOT_TIMEOUT,
    CONF_RETAIN,
    CONF_SHUTDOWN_MESSAGE,
    CONF_SKIP_CERT_CN_CHECK,
    CONF_STATE_TOPIC,
    CONF_SUBSCRIBE_QOS,
    CONF_TARGET_TEMPERATURE_COMMAND_TOPIC,
    CONF_TARGET_TEMPERATURE_HIGH_COMMAND_TOPIC,
    CONF_TARGET_TEMPERATURE_LOW_COMMAND_TOPIC,
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
from esphome.core import CORE, ID, CoroPriority, Lambda, coroutine_with_priority
from esphome.cpp_generator import MockObj, MockObjClass
import esphome.final_validate as fv
from esphome.helpers import sanitize, snake_case
from esphome.mqtt_topics import (
    DEFAULT_TOPIC_SUFFIXES,
    per_board_suffix,
    prefix_gets_mac_suffix,
)
from esphome.types import ConfigType, TemplateArgsType

DEPENDENCIES = ["network"]
DOMAIN = "mqtt"


def AUTO_LOAD():
    if CORE.is_esp8266 or CORE.is_libretiny:
        return ["async_tcp", "json"]
    return ["json"]


CONF_TLS = "tls"
CONF_IDF_SEND_ASYNC = "idf_send_async"
CONF_WAIT_FOR_CONNECTION = "wait_for_connection"
CONF_PERSIST_SUBSCRIPTIONS = "persist_subscriptions"
CONF_TRANSPORT = "transport"
CONF_WS_PATH = "ws_path"

MQTT_TRANSPORT_TCP = "tcp"
MQTT_TRANSPORT_WS = "ws"
MQTT_TRANSPORT_WSS = "wss"
DEFAULT_WS_PATH = "/mqtt"

# Max lengths for stack-based topic building.
# These values are used in cv.Length() validators below to ensure the C++ code
# can safely use fixed-size stack buffers without overflow. If you change these,
# update the corresponding constants in mqtt_client.h and mqtt_component.h.
TOPIC_PREFIX_MAX_LEN = 64  # Default is device name, typically short
DISCOVERY_PREFIX_MAX_LEN = 64  # Default is "homeassistant" (13 chars)
# Size of the stack buffer topics are built into (static_assert in mqtt_client.cpp)
MQTT_DEFAULT_TOPIC_MAX_LEN = 248


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


def _topic_source(topic: str) -> cg.MockObj:
    # On ESP8266 a topic that fits the buffer stays in flash and is copied out when needed
    if CORE.is_esp8266 and len(topic.encode()) < MQTT_DEFAULT_TOPIC_MAX_LEN:
        return mqtt_ns.progmem_topic(cg.FlashStringLiteral(topic))
    return mqtt_ns.static_topic(topic)


MQTTMessage = mqtt_ns.struct("MQTTMessage")
MQTTClientDisconnectReason = mqtt_ns.enum("MQTTClientDisconnectReason")
MQTTClientComponent = mqtt_ns.class_("MQTTClientComponent", cg.Component)
MQTTComponent = mqtt_ns.class_("MQTTComponent", cg.Component)

# A topic the firmware builds from a suffix, unless the config key replaces the whole topic
_Topic = tuple[str, str]
# The firmware subscribes to exactly one of these alternatives, chosen by entity traits
_Subscription = tuple[tuple[_Topic, ...], ...]
# The subscriptions each entity MQTT class always makes, keyed by class: component type,
# then the subscriptions
_COMMAND_TOPICS: dict[str, tuple[str, tuple[_Subscription, ...]]] = {}


def _single_topic(suffix: str, custom_key: str) -> _Subscription:
    return (((suffix, custom_key),),)


_COMMAND = _single_topic("command", CONF_COMMAND_TOPIC)


def _entity_class(
    name: str,
    component_type: str | None = None,
    subscriptions: tuple[_Subscription, ...] = (_COMMAND,),
) -> MockObjClass:
    """Declare an entity MQTT class; a component_type means it subscribes to command topics."""
    cls = mqtt_ns.class_(name, MQTTComponent)
    if component_type is not None:
        _COMMAND_TOPICS[str(cls)] = (component_type, subscriptions)
    return cls


MQTTAlarmControlPanelComponent = _entity_class(
    "MQTTAlarmControlPanelComponent", "alarm_control_panel"
)
MQTTBinarySensorComponent = _entity_class("MQTTBinarySensorComponent")
MQTTClimateComponent = _entity_class(
    "MQTTClimateComponent",
    "climate",
    (
        _single_topic("mode/command", CONF_MODE_COMMAND_TOPIC),
        # One target temperature topic, or the _low and _high pair on two-point devices
        (
            (("target_temperature/command", CONF_TARGET_TEMPERATURE_COMMAND_TOPIC),),
            (
                (
                    "target_temperature_low/command",
                    CONF_TARGET_TEMPERATURE_LOW_COMMAND_TOPIC,
                ),
                (
                    "target_temperature_high/command",
                    CONF_TARGET_TEMPERATURE_HIGH_COMMAND_TOPIC,
                ),
            ),
        ),
    ),
)
MQTTCoverComponent = _entity_class("MQTTCoverComponent", "cover")
MQTTFanComponent = _entity_class("MQTTFanComponent", "fan")
MQTTJSONLightComponent = _entity_class("MQTTJSONLightComponent", "light")
MQTTSensorComponent = _entity_class("MQTTSensorComponent")
MQTTSwitchComponent = _entity_class("MQTTSwitchComponent", "switch")
MQTTTextSensor = _entity_class("MQTTTextSensor")
MQTTNumberComponent = _entity_class("MQTTNumberComponent", "number")
MQTTDateComponent = _entity_class("MQTTDateComponent", "date")
MQTTTimeComponent = _entity_class("MQTTTimeComponent", "time")
MQTTDateTimeComponent = _entity_class("MQTTDateTimeComponent", "datetime")
MQTTTextComponent = _entity_class("MQTTTextComponent", "text")
MQTTSelectComponent = _entity_class("MQTTSelectComponent", "select")
MQTTButtonComponent = _entity_class("MQTTButtonComponent", "button")
MQTTLockComponent = _entity_class("MQTTLockComponent", "lock")
MQTTEventComponent = _entity_class("MQTTEventComponent")
MQTTUpdateComponent = _entity_class("MQTTUpdateComponent", "update")
MQTTValveComponent = _entity_class("MQTTValveComponent", "valve")

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


# (config key, extra fields when defaulted from the topic prefix, (setter, disabler))
_MESSAGES = (
    (
        CONF_BIRTH_MESSAGE,
        {CONF_PAYLOAD: "online"},
        lambda var: (var.set_birth_message, var.disable_birth_message),
    ),
    (
        CONF_WILL_MESSAGE,
        {CONF_PAYLOAD: "offline"},
        lambda var: (var.set_last_will, var.disable_last_will),
    ),
    (
        CONF_SHUTDOWN_MESSAGE,
        {CONF_PAYLOAD: "offline"},
        lambda var: (var.set_shutdown_message, var.disable_shutdown_message),
    ),
    (
        CONF_LOG_TOPIC,
        {},
        lambda var: (var.set_log_message_template, var.disable_log_message),
    ),
)


def validate_config(value):
    if (
        value.get(CONF_TRANSPORT) not in (MQTT_TRANSPORT_WS, MQTT_TRANSPORT_WSS)
        and CONF_WS_PATH in value
    ):
        raise cv.Invalid(
            f"'{CONF_WS_PATH}' is only valid when 'transport' is 'ws' or 'wss'"
        )
    if value.get(CONF_PERSIST_SUBSCRIPTIONS) and value.get(CONF_CLEAN_SESSION):
        raise cv.Invalid(
            f"'{CONF_PERSIST_SUBSCRIPTIONS}' requires '{CONF_CLEAN_SESSION}: false'"
        )

    # Populate default fields
    out = value.copy()
    topic_prefix = value[CONF_TOPIC_PREFIX]
    # If the topic prefix is not null and these messages are not configured, then set them to the default
    # If the topic prefix is null and these messages are not configured, then set them to null
    for key, extra, _ in _MESSAGES:
        if key in value:
            continue
        out[key] = (
            {}
            if topic_prefix == ""
            else {
                CONF_TOPIC: f"{topic_prefix}{DEFAULT_TOPIC_SUFFIXES[key]}",
                **extra,
                CONF_QOS: 0,
                CONF_RETAIN: True,
            }
        )
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
            cv.Optional(CONF_PERSIST_SUBSCRIPTIONS, default=False): cv.boolean,
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

# AsyncMqttClient, the ESP8266 and LibreTiny backend, drops any inbound message whose
# topic is longer than this many bytes (its default maxTopicLength)
ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN = 128
# Keep in sync with OBJECT_ID_MAX_LEN in esphome/core/entity_base.h
OBJECT_ID_MAX_LEN = 128
# name_add_mac_suffix appends "-" plus 6 hex digits to the name at runtime
MAC_SUFFIX = "-xxxxxx"


def _entity_object_id(
    config: ConfigType, mac_suffix: bool, devices: dict[str, str]
) -> str:
    """Build the object id the way EntityBase::write_object_id_to() does."""
    # The name falls back the way EntityBase::configure_entity_() does
    name = config.get(CONF_NAME) or ""
    if not name:
        if (device := config.get(CONF_DEVICE_ID)) is not None:
            name = devices[device.id]
        elif mac_suffix:
            # Only the friendly name gains the MAC suffix, and an empty one stays empty
            name = f"{CORE.friendly_name}{MAC_SUFFIX}" if CORE.friendly_name else ""
        else:
            name = CORE.friendly_name or CORE.name
    # The firmware sanitizes each UTF-8 byte to one character and caps the result
    raw = name.encode()[: OBJECT_ID_MAX_LEN - 1]
    return sanitize(snake_case(raw.decode("ascii", "replace")))


def _entity_topic(
    config: ConfigType, default_topic: str, suffix: str, custom_key: str
) -> str | None:
    """Return the topic an entity subscribes to, or None when only known at runtime."""
    custom = config.get(custom_key)
    # An empty specialized topic falls back to the default (MQTT_COMPONENT_CUSTOM_TOPIC_)
    # while an empty command_topic stays empty
    if custom == "" and custom_key != CONF_COMMAND_TOPIC:
        custom = None
    if custom is not None:
        # A lambda topic is only known at runtime
        return custom if isinstance(custom, str) else None
    return f"{default_topic}/{suffix}" if default_topic else None


def _entity_subscriptions(
    config: ConfigType, topic_prefix: str, mac_suffix: bool, devices: dict[str, str]
) -> Iterator[tuple[tuple[str, ...], ...]]:
    """Yield the subscriptions an entity's MQTT component always makes, as known topics."""
    component_type, subscriptions = _COMMAND_TOPICS[str(config[CONF_MQTT_ID].type)]
    # Mirrors MQTTComponent::compute_is_internal_(): internal components never subscribe
    if CONF_STATE_TOPIC in config:
        internal = config[CONF_STATE_TOPIC] == ""
    elif CONF_COMMAND_TOPIC in config:
        internal = config[CONF_COMMAND_TOPIC] == ""
    else:
        internal = not topic_prefix or config.get(CONF_INTERNAL, False)
    if internal:
        return
    default_topic = ""
    if topic_prefix:
        object_id = _entity_object_id(config, mac_suffix, devices)
        default_topic = f"{topic_prefix}/{component_type}/{object_id}"
    for subscription in subscriptions:
        yield tuple(
            tuple(
                topic
                for suffix, custom_key in alternative
                if (topic := _entity_topic(config, default_topic, suffix, custom_key))
                is not None
            )
            for alternative in subscription
        )


def _subscriptions(config: ConfigType) -> Iterator[tuple[tuple[str, ...], ...]]:
    """Yield every subscription known at build time, as alternatives of topics."""
    full_config = fv.full_config.get()
    esphome_config = full_config[CONF_ESPHOME]
    mac_suffix = esphome_config.get(CONF_NAME_ADD_MAC_SUFFIX, False)
    devices = {
        device[CONF_ID].id: device[CONF_NAME]
        for device in esphome_config.get(CONF_DEVICES, [])
    }
    topic_prefix = config[CONF_TOPIC_PREFIX]
    if prefix_gets_mac_suffix(topic_prefix, CORE.name, mac_suffix):
        topic_prefix += MAC_SUFFIX
    # Entities can sit at any depth (a platform can declare its own switches), so find them by mqtt_id
    for id_, path in iter_ids(full_config):
        if path[-1] != CONF_MQTT_ID or str(id_.type) not in _COMMAND_TOPICS:
            continue
        entity = full_config
        for key in path[:-1]:
            entity = entity[key]
        yield from _entity_subscriptions(entity, topic_prefix, mac_suffix, devices)
    for key in (CONF_ON_MESSAGE, CONF_ON_JSON_MESSAGE):
        for trigger in config.get(key, []):
            yield ((trigger[CONF_TOPIC],),)
    for domain in ("sensor", "text_sensor"):
        for entity in full_config.get(domain) or []:
            if entity[CONF_PLATFORM] == "mqtt_subscribe":
                yield ((entity[CONF_TOPIC],),)


def _too_long(topic: str) -> bool:
    # A wildcard filter matches topics of any length, so only literals are checked
    if "+" in topic or "#" in topic:
        return False
    return len(topic.encode()) > ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN


def _validate_topic_lengths(config: ConfigType) -> ConfigType:
    """Reject topics that the ESP8266 and LibreTiny MQTT client would silently drop."""
    if not (CORE.is_esp8266 or CORE.is_libretiny):
        return config
    for subscription in _subscriptions(config):
        # The config cannot tell which alternative the firmware picks, so only a
        # subscription that is too long whichever way is rejected
        too_long = [
            next((topic for topic in alternative if _too_long(topic)), None)
            for alternative in subscription
        ]
        if all(too_long):
            topic = too_long[0]
            raise cv.Invalid(
                f"MQTT topic '{topic}' is {len(topic.encode())} bytes; the MQTT client "
                f"on this platform drops topics longer than {ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN}"
            )
    return config


FINAL_VALIDATE_SCHEMA = _validate_topic_lengths


def exp_mqtt_message(
    config: ConfigType | None, topic: cg.MockObj | None = None
) -> cg.Expression:
    if config is None:
        return cg.optional(cg.TemplateArguments(MQTTMessage))
    return cg.StructInitializer(
        MQTTMessage,
        ("topic", config[CONF_TOPIC] if topic is None else topic),
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
    if config[CONF_PERSIST_SUBSCRIPTIONS]:
        cg.add_define("USE_MQTT_SESSION_PERSISTENCE")
        # The session fingerprint, a uint32_t kept in RTC memory
        preferences.reserve_preferences(1, 4, in_flash=False)
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

    topic_prefix = config[CONF_TOPIC_PREFIX]
    # The MAC suffix is only known at runtime, so the prefix and the topics defaulted from it
    # are built from App.get_name(); topics written out by hand stay literal
    mac_suffix = CORE.config[CONF_ESPHOME].get(CONF_NAME_ADD_MAC_SUFFIX, False)
    mac_prefix = prefix_gets_mac_suffix(topic_prefix, CORE.name, mac_suffix)
    cg.add(var.set_topic_prefix(cg.App.get_name() if mac_prefix else topic_prefix))

    if config[CONF_USE_ABBREVIATIONS]:
        cg.add_define("USE_MQTT_ABBREVIATIONS")

    for key, _, methods in _MESSAGES:
        set_message, disable_message = methods(var)
        if not (message := config[key]):
            cg.add(disable_message())
            continue
        topic = (
            cg.App.get_name() + suffix
            if (suffix := per_board_suffix(config, key, CORE.name, mac_suffix))
            else None
        )
        cg.add(set_message(exp_mqtt_message(message, topic)))

    if log_topic := config[CONF_LOG_TOPIC]:
        # Request a log listener slot only when log topic is enabled
        logger.request_log_listener()
        if (level := log_topic.get(CONF_LEVEL)) is not None:
            cg.add(var.set_log_level(logger.LOG_LEVELS[level]))

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
        cg.add(var.subscribe(_topic_source(conf[CONF_TOPIC]), callback, conf[CONF_QOS]))

    for conf in config.get(CONF_ON_JSON_MESSAGE, []):
        callback = await automation.build_trigger_callback(
            [(cg.JsonObjectConst, "x")],
            conf,
            params=[(cg.std_string, "topic"), (cg.JsonObject, "root")],
            forward=["root"],
        )
        cg.add(
            var.subscribe_json(
                _topic_source(conf[CONF_TOPIC]), callback, conf[CONF_QOS]
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
    automation.ApplyAction,
    MQTT_PUBLISH_JSON_ACTION_SCHEMA,
    synchronous=True,
)
async def mqtt_publish_json_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    parent = automation.parent_ref(await cg.get_variable(config[CONF_ID]))
    # Lambdas are called in order before the publish, with the trigger args by value
    statements: list[str] = []
    values: dict[str, str] = {}
    for key, type_, const_fn in (
        (CONF_TOPIC, cg.std_string, automation.flash_string),
        (CONF_QOS, cg.uint8, None),
        (CONF_RETAIN, cg.bool_, None),
    ):
        if not isinstance(value := config[key], Lambda):
            values[key] = (
                const_fn(config, value) if const_fn else str(cg.safe_exp(value))
            )
            continue
        lambda_ = await cg.process_lambda(value, args, return_type=type_)
        statements.append(
            f"const {type_} esphome__{key} = {automation.immediate_lambda_call(lambda_)};"
        )
        values[key] = f"esphome__{key}"
    payload = await cg.process_lambda(
        config[CONF_PAYLOAD], [*args, (cg.JsonObject, "root")], return_type=cg.void
    )
    statements.append(
        f"{parent}->publish_json({values[CONF_TOPIC]}, [&]({cg.JsonObject} root) {{ "
        f"{automation.immediate_lambda_call(payload)}; }}, "
        f"{values[CONF_QOS]}, {values[CONF_RETAIN]});"
    )
    return automation.apply_statements_action(action_id, template_arg, args, statements)


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
