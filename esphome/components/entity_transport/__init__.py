"""Send sensor and binary sensor states to other devices over a packet interface."""

from collections.abc import Callable
from typing import Any

import esphome.codegen as cg
from esphome.components import packet_interface
from esphome.components.binary_sensor import BinarySensor
from esphome.components.packet_interface import CONF_PACKET_INTERFACE
from esphome.components.sensor import Sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINARY_SENSORS,
    CONF_ID,
    CONF_SENSORS,
    CONF_TIMEOUT,
    CONF_UPDATE_INTERVAL,
)
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["packet_interface"]
DOMAIN = "entity_transport"
MULTI_CONF = True

CONF_BROADCAST_ID = "broadcast_id"
CONF_ENTITY_TRANSPORT_ID = "entity_transport_id"
CONF_PROVIDER = "provider"
CONF_REMOTE_ID = "remote_id"

entity_transport_ns = cg.esphome_ns.namespace("entity_transport")
EntityTransport = entity_transport_ns.class_("EntityTransport", cg.Component)

_request_remote_sensor_slot = cg.slot_counter("ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT")
_request_remote_binary_sensor_slot = cg.slot_counter(
    "ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT"
)


def _local_entity(cls: MockObjClass) -> Callable[[Any], Any]:
    return cv.maybe_simple_value(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(cls),
                cv.Optional(CONF_BROADCAST_ID): cv.All(
                    cv.validate_id_name, cv.Length(max=255)
                ),
            }
        ),
        key=CONF_ID,
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EntityTransport),
        cv.Required(CONF_PACKET_INTERFACE): cv.use_id(packet_interface.PacketInterface),
        # Defaults to a periodic resend unless the interface has session and reliability layers
        cv.Optional(CONF_UPDATE_INTERVAL): cv.All(
            cv.update_interval, cv.Range(min=cv.TimePeriod(seconds=1))
        ),
        # Remote entities lose their state when none arrives for this long
        cv.Optional(CONF_TIMEOUT): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(seconds=1)),
        ),
        # Entities are numbered with one byte
        cv.Optional(CONF_SENSORS): cv.All(
            cv.ensure_list(_local_entity(Sensor)), cv.Length(max=255)
        ),
        cv.Optional(CONF_BINARY_SENSORS): cv.All(
            cv.ensure_list(_local_entity(BinarySensor)), cv.Length(max=255)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_remote_id(config: ConfigType) -> ConfigType:
    if CONF_REMOTE_ID not in config and not config[CONF_ID].is_manual:
        raise cv.Invalid(f"'{CONF_REMOTE_ID}' is required unless '{CONF_ID}' is given")
    return config


def remote_entity_schema(base_schema: cv.Schema) -> cv.Schema:
    """Schema for an entity whose state comes from another device."""
    return cv.All(
        base_schema.extend(
            {
                cv.GenerateID(CONF_ENTITY_TRANSPORT_ID): cv.use_id(EntityTransport),
                cv.Required(CONF_PROVIDER): cv.valid_name,
                cv.Optional(CONF_REMOTE_ID): cv.All(
                    cv.string_strict, cv.Length(min=1, max=255)
                ),
            }
        ),
        _validate_remote_id,
    )


def _remote_id(config: ConfigType) -> str:
    return config.get(CONF_REMOTE_ID) or config[CONF_ID].id


async def register_remote_sensor(config: ConfigType, var: MockObj) -> None:
    hub = await cg.get_variable(config[CONF_ENTITY_TRANSPORT_ID])
    _request_remote_sensor_slot(str(hub))
    cg.add(hub.add_remote_sensor(config[CONF_PROVIDER], _remote_id(config), var))


async def register_remote_binary_sensor(config: ConfigType, var: MockObj) -> None:
    hub = await cg.get_variable(config[CONF_ENTITY_TRANSPORT_ID])
    _request_remote_binary_sensor_slot(str(hub))
    cg.add(hub.add_remote_binary_sensor(config[CONF_PROVIDER], _remote_id(config), var))


async def to_code(config: ConfigType) -> None:
    interface = await cg.get_variable(config[CONF_PACKET_INTERFACE])
    var = cg.new_Pvariable(config[CONF_ID], interface)
    await cg.register_component(var, config)
    if (interval := config.get(CONF_UPDATE_INTERVAL)) is not None:
        cg.add(var.set_update_interval(interval))
    if (timeout := config.get(CONF_TIMEOUT)) is not None:
        cg.add(var.set_remote_timeout(timeout))
    sensors = config.get(CONF_SENSORS, [])
    binary_sensors = config.get(CONF_BINARY_SENSORS, [])
    if count := len(sensors) + len(binary_sensors):
        cg.add(var.set_entity_count(count))
    for conf in sensors:
        sensor = await cg.get_variable(conf[CONF_ID])
        cg.add(var.add_sensor(conf.get(CONF_BROADCAST_ID, conf[CONF_ID].id), sensor))
    for conf in binary_sensors:
        sensor = await cg.get_variable(conf[CONF_ID])
        cg.add(
            var.add_binary_sensor(conf.get(CONF_BROADCAST_ID, conf[CONF_ID].id), sensor)
        )
