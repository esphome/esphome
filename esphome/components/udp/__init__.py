import hashlib

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.api import CONF_ENCRYPTION
from esphome.components.binary_sensor import BinarySensor
from esphome.components.sensor import Sensor
from esphome.const import (
    CONF_PORT,
    CONF_SENSORS,
    CONF_ID,
    CONF_BINARY_SENSORS,
    CONF_NAME,
    CONF_KEY,
    CONF_INTERNAL,
)
from esphome.cpp_generator import MockObjClass

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["socket"]
MULTI_CONF = True

udp_ns = cg.esphome_ns.namespace("udp")
UDPComponent = udp_ns.class_("UDPComponent", cg.PollingComponent)

CONF_BROADCAST = "broadcast"
CONF_ADDRESSES = "addresses"
CONF_PROVIDER = "provider"
CONF_PROVIDERS = "providers"
CONF_REMOTE_ID = "remote_id"
CONF_UDP_ID = "udp_id"
CONF_PING_PONG_ENABLE = "ping_pong_enable"
CONF_PING_PONG_RECYCLE_TIME = "ping_pong_recycle_time"
CONF_ROLLING_CODE_ENABLE = "rolling_code_enable"


def sensor_validation(cls: MockObjClass):
    def validator(value):
        value = cv.use_id(cls)(value)
        if len(value.id) > 128:
            raise cv.Invalid("sensor id must be <= 128 bytes long")
        return value

    return validator


ENCRYPTION_SCHEMA = {
    cv.Optional(CONF_ENCRYPTION): cv.maybe_simple_value(
        cv.Schema(
            {
                cv.Required(CONF_KEY): cv.string,
            }
        ),
        key=CONF_KEY,
    )
}

PROVIDER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.valid_name,
    }
).extend(ENCRYPTION_SCHEMA)


def validate_(config):
    if CONF_ENCRYPTION in config:
        if CONF_SENSORS not in config and CONF_BINARY_SENSORS not in config:
            raise cv.Invalid("No sensors or binary sensors to encrypt")
    elif config[CONF_ROLLING_CODE_ENABLE]:
        raise cv.Invalid("Rolling code requires an encryption key")
    if config[CONF_PING_PONG_ENABLE]:
        if not any(CONF_ENCRYPTION in p for p in config.get(CONF_PROVIDERS) or ()):
            raise cv.Invalid("Ping-pong requires at least one encrypted provider")
    return config


CONFIG_SCHEMA = cv.All(
    cv.polling_component_schema("15s")
    .extend(
        {
            cv.GenerateID(): cv.declare_id(UDPComponent),
            cv.Optional(CONF_PORT, default=18511): cv.port,
            cv.Optional(CONF_ADDRESSES): cv.ensure_list(cv.ipv4),
            cv.Optional(CONF_ROLLING_CODE_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_PING_PONG_ENABLE, default=False): cv.boolean,
            cv.Optional(
                CONF_PING_PONG_RECYCLE_TIME, default="600s"
            ): cv.positive_time_period_seconds,
            cv.Optional(CONF_SENSORS): cv.ensure_list(sensor_validation(Sensor)),
            cv.Optional(CONF_BINARY_SENSORS): cv.ensure_list(
                sensor_validation(BinarySensor)
            ),
            cv.Optional(CONF_PROVIDERS): cv.ensure_list(PROVIDER_SCHEMA),
        },
    )
    .extend(ENCRYPTION_SCHEMA),
    validate_,
)

SENSOR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_REMOTE_ID): cv.string_strict,
        cv.Required(CONF_PROVIDER): cv.valid_name,
        cv.GenerateID(CONF_UDP_ID): cv.use_id(UDPComponent),
    }
)


def require_internal_with_name(config):
    if CONF_NAME in config and CONF_INTERNAL not in config:
        raise cv.Invalid("Must provide internal: config when using name:")
    return config


def hash_encryption_key(config: dict):
    return list(hashlib.sha256(config[CONF_KEY].encode()).digest())


async def to_code(config):
    cg.add_define("USE_UDP")
    cg.add_global(udp_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_rolling_code_enable(config[CONF_ROLLING_CODE_ENABLE]))
    cg.add(var.set_ping_pong_enable(config[CONF_PING_PONG_ENABLE]))
    cg.add(
        var.set_ping_pong_recycle_time(
            config[CONF_PING_PONG_RECYCLE_TIME].total_seconds
        )
    )
    if sensors := config.get(CONF_SENSORS):
        for sensor_id in sensors:
            sensor = await cg.get_variable(sensor_id)
            cg.add(var.add_sensor(sensor_id.id, sensor))
    if sensors := config.get(CONF_BINARY_SENSORS):
        for sensor_id in sensors:
            sensor = await cg.get_variable(sensor_id)
            cg.add(var.add_binary_sensor(sensor_id.id, sensor))
    if addresses := config.get(CONF_ADDRESSES):
        for address in addresses:
            cg.add(var.add_address(str(address)))
    else:
        cg.add(var.add_address("255.255.255.255"))

    if encryption := config.get(CONF_ENCRYPTION):
        cg.add(var.set_encryption_key(hash_encryption_key(encryption)))

    if providers := config.get(CONF_PROVIDERS):
        for provider in providers:
            name = provider[CONF_NAME]
            cg.add(var.add_provider(name))
            if encryption := provider.get(CONF_ENCRYPTION):
                cg.add(
                    var.set_provider_encryption(name, hash_encryption_key(encryption))
                )
