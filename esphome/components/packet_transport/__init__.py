import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor
from esphome.const import (
    CONF_BINARY_SENSORS,
    CONF_KEY,
    CONF_NAME,
    CONF_SENSORS,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import CORE

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = []
AUTO_LOAD = ["sensor", "binary_sensor"]

packet_transport_ns = cg.esphome_ns.namespace("packet_transport")
PacketTransport = packet_transport_ns.class_("PacketTransport", cg.PollingComponent)

# Component-specific constants (not in esphome.const)
CONF_BROADCAST = "broadcast"
CONF_ENCRYPTION = "encryption"
CONF_PING_PONG = "ping_pong"
CONF_PING_PONG_ENABLE = "ping_pong_enable"
CONF_PROVIDER = "provider"
CONF_PROVIDERS = "providers"
CONF_REMOTE_ID = "remote_id"
CONF_ROLLING_CODE_ENABLE = "rolling_code_enable"
CONF_STATUS_SENSOR = "status_sensor"
CONF_TRANSPORT_ID = "transport_id"


def sensor_validation(config):
    if (
        CONF_BROADCAST in config
        and config[CONF_BROADCAST] is not None
        and CONF_REMOTE_ID in config
    ):
        raise cv.Invalid("Cannot specify both broadcast and remote_id")
    return config


def provider_name_validate(value):
    value = cv.string(value)
    if "_" in value:
        raise cv.Invalid(
            "Provider names should not contain underscores. Use hyphens instead."
        )
    return value


TRANSPORT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PacketTransport),
        cv.Optional(CONF_UPDATE_INTERVAL, default="15s"): cv.update_interval,
        cv.Optional(CONF_SENSORS): cv.ensure_list(sensor.sensor_schema()),
        cv.Optional(CONF_BINARY_SENSORS): cv.ensure_list(
            binary_sensor.binary_sensor_schema()
        ),
        cv.Optional(CONF_PROVIDERS): cv.ensure_list(
            {
                cv.Required(CONF_NAME): provider_name_validate,
                cv.Optional(CONF_KEY): cv.string,
                cv.Optional(CONF_PING_PONG, default=False): cv.boolean,
            }
        ),
        cv.Optional(CONF_KEY): cv.string,
    }
)


def validate_(config):
    if CONF_ENCRYPTION not in config and CONF_KEY in config:
        config = config.copy()
        config[CONF_ENCRYPTION] = {CONF_KEY: config.pop(CONF_KEY)}
    if CONF_ENCRYPTION in config:
        providers = config.get(CONF_PROVIDERS, [])
        for provider in providers:
            if (
                CONF_PING_PONG in provider
                and provider[CONF_PING_PONG]
                and CONF_KEY not in config[CONF_ENCRYPTION]
                and CONF_KEY not in provider
            ):
                raise cv.Invalid(
                    f"Ping pong requires encryption key for provider {provider[CONF_NAME]}"
                )
    return config


def packet_transport_sensor_schema(schema):
    return (
        schema.extend(
            {
                cv.GenerateID(CONF_TRANSPORT_ID): cv.use_id(PacketTransport),
                cv.Required(CONF_PROVIDER): provider_name_validate,
                cv.Optional(CONF_REMOTE_ID): cv.string,
                cv.Optional(CONF_BROADCAST): cv.uint8_t,
            }
        )
        .add_extra(sensor_validation)
    )


async def register_packet_transport(config, transport):
    await cg.register_component(transport, config)
    cg.add(transport.set_update_interval(config[CONF_UPDATE_INTERVAL]))

    # Collect all providers
    providers = {}
    for provider in config.get(CONF_PROVIDERS, []):
        providers[provider[CONF_NAME]] = provider

    for sens in config.get(CONF_SENSORS, []):
        var = await sensor.new_sensor(sens)
        cg.add(transport.add_sensor(var))
        provider = sens.get(CONF_PROVIDER)
        if provider is not None:
            providers.setdefault(provider, {CONF_NAME: provider})

    for sens in config.get(CONF_BINARY_SENSORS, []):
        var = await binary_sensor.new_binary_sensor(sens)
        cg.add(transport.add_binary_sensor(var))
        provider = sens.get(CONF_PROVIDER)
        if provider is not None:
            providers.setdefault(provider, {CONF_NAME: provider})

    if CONF_ENCRYPTION in config:
        enc = config[CONF_ENCRYPTION]
        if CONF_KEY in enc:
            key = CORE.safe_exp(enc[CONF_KEY])
            key = cg.RawExpression(f"esp_hash_impl({key})")
            cg.add(transport.set_encryption_key(cg.RawExpression(f"(const uint8_t*){key}"), 32))

    for provider in providers.values():
        name = provider[CONF_NAME]
        cg.add(transport.add_provider(name))
        if CONF_KEY in provider:
            key = CORE.safe_exp(provider[CONF_KEY])
            key = cg.RawExpression(f"esp_hash_impl({key})")
            cg.add(transport.set_provider_encryption(name, cg.RawExpression(f"(const uint8_t*){key}"), 32))
