"""Route/port counter and gauge sensors for can_gateway."""

from __future__ import annotations

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_PERCENT,
)

from . import CONF_PORT_ID, CONF_ROUTE_ID, GatewayPort, GatewayRoute, can_gateway_ns

CanGatewaySensorHub = can_gateway_ns.class_("CanGatewaySensorHub", cg.PollingComponent)

# Order defines the C++ `kind` index passed to set_counter_sensor(); it must
# match the KindIndex enum in can_gateway.h exactly. Append only — inserting
# would silently shift every later sensor onto the wrong counter. The order
# is locked by a test in tests/component_tests/can_gateway/test_entities.py.
ROUTE_COUNTERS = ("forwarded", "filtered", "tx_full", "bus_off", "disabled")
PORT_COUNTERS = ("injected", "tx_fail", "bus_err", "recoveries")
PORT_GAUGES = ("tec", "rec")
# Statistics gauges compile in the ISR bit accounting; zero cost when
# no such sensor is configured.
PORT_STATS = ("bus_load",)
ALL_KINDS = ROUTE_COUNTERS + PORT_COUNTERS + PORT_GAUGES + PORT_STATS


def _counter_schema():
    return sensor.sensor_schema(
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        accuracy_decimals=0,
        icon="mdi:counter",
    )


def _gauge_schema():
    return sensor.sensor_schema(
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        accuracy_decimals=0,
        icon="mdi:alert-circle-outline",
    )


def _percent_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        accuracy_decimals=1,
        icon="mdi:gauge",
    )


def _validate_kinds(config):
    """V8: sub-sensors must match the owner kind (route vs port)."""
    configured = [kind for kind in ALL_KINDS if kind in config]
    if not configured:
        raise cv.Invalid("configure at least one counter or gauge sensor")
    if CONF_ROUTE_ID in config:
        for kind in configured:
            if kind not in ROUTE_COUNTERS:
                raise cv.Invalid(
                    f"'{kind}' is a port sensor; use port_id instead of route_id"
                )
    else:
        for kind in configured:
            if kind in ROUTE_COUNTERS:
                raise cv.Invalid(
                    f"'{kind}' is a route sensor; use route_id instead of port_id"
                )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CanGatewaySensorHub),
            cv.Optional(CONF_ROUTE_ID): cv.use_id(GatewayRoute),
            cv.Optional(CONF_PORT_ID): cv.use_id(GatewayPort),
            **{cv.Optional(kind): _counter_schema() for kind in ROUTE_COUNTERS},
            **{cv.Optional(kind): _counter_schema() for kind in PORT_COUNTERS},
            **{cv.Optional(kind): _gauge_schema() for kind in PORT_GAUGES},
            **{cv.Optional(kind): _percent_schema() for kind in PORT_STATS},
        }
    ).extend(cv.polling_component_schema("60s")),
    cv.has_exactly_one_key(CONF_ROUTE_ID, CONF_PORT_ID),
    _validate_kinds,
)


async def to_code(config):
    from esphome.const import CONF_ID

    hub = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(hub, config)
    if (route_id := config.get(CONF_ROUTE_ID)) is not None:
        cg.add(hub.set_route(await cg.get_variable(route_id)))
    if (port_id := config.get(CONF_PORT_ID)) is not None:
        cg.add(hub.set_port(await cg.get_variable(port_id)))
    for kind_index, kind in enumerate(ALL_KINDS):
        if (conf := config.get(kind)) is not None:
            if kind in PORT_STATS:
                # Compiles the ISR-side bit accounting in; without any
                # statistics gauge the fast path carries zero cost.
                cg.add_define("USE_CAN_GATEWAY_STATS")
            sens = await sensor.new_sensor(conf)
            cg.add(hub.set_counter_sensor(kind_index, sens))
