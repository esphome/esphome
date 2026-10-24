"""Session layer that sends keep-alives and reports whether the peer can be reached."""

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINARY_SENSOR,
    CONF_ID,
    CONF_TIMEOUT,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import SESSION, PacketInterface, packet_interface_ns

CONF_KEEP_ALIVE = "keep_alive"

BasicSession = packet_interface_ns.class_("BasicSession", PacketInterface)


def _session_timeout(config: ConfigType) -> ConfigType:
    """Default the timeout to three keep-alive intervals, and require it to be longer than one."""
    keep_alive = config[CONF_KEEP_ALIVE].total_milliseconds
    if (timeout := config.get(CONF_TIMEOUT)) is None:
        config[CONF_TIMEOUT] = cv.positive_time_period_milliseconds(
            f"{keep_alive * 3}ms"
        )
    elif timeout.total_milliseconds <= keep_alive:
        raise cv.Invalid("Must be longer than keep_alive", path=[CONF_TIMEOUT])
    return config


BASIC_SESSION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BasicSession),
            cv.Optional(CONF_KEEP_ALIVE, default="10s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(seconds=1), max=cv.TimePeriod(hours=1)),
            ),
            cv.Optional(CONF_TIMEOUT): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=cv.TimePeriod(hours=3)),
            ),
            cv.Optional(CONF_BINARY_SENSOR): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _session_timeout,
)


@SESSION.register(
    "basic",
    schema=BASIC_SESSION_SCHEMA,
    auto_load=lambda config: ["binary_sensor"] if CONF_BINARY_SENSOR in config else [],
)
async def build_basic_session(config: ConfigType, lower: MockObj) -> MockObj:
    cg.add_define("USE_PACKET_INTERFACE_BASIC_SESSION")
    var = cg.new_Pvariable(config[CONF_ID], lower)
    cg.add(var.set_keep_alive(config[CONF_KEEP_ALIVE]))
    cg.add(var.set_session_timeout(config[CONF_TIMEOUT]))
    await cg.register_component(var, config)
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_BINARY_SENSOR, var.set_connected_sensor)
    return var
