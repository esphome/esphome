"""Schema tests for the can_gateway entity platforms (cfg-v08)."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv

from .common import gateway, setup_c6


def _validated_gateway(set_core_config):
    from esphome.components.can_gateway import CONFIG_SCHEMA

    setup_c6(set_core_config)
    return CONFIG_SCHEMA(
        gateway(routes=[{"id": "route_ab", "from": "port_a", "to": "port_b"}])
    )


# ------------------------------------------------------------------ sensor


def test_sensor_kind_order_locked() -> None:
    """ALL_KINDS defines the positional `kind` indices handed to C++.

    The order must match the KindIndex enum in can_gateway.h exactly; this
    test freezes the Python side so an accidental insertion (which would
    silently shift every later sensor onto the wrong counter) fails loudly.
    Append new kinds at the end of BOTH sides.
    """
    from esphome.components.can_gateway.sensor import ALL_KINDS

    assert ALL_KINDS == (
        "forwarded",
        "filtered",
        "tx_full",
        "bus_off",
        "disabled",
        "injected",
        "tx_fail",
        "bus_err",
        "recoveries",
        "tec",
        "rec",
        "bus_load",
    )


def test_v08_route_counters_accepted(set_core_config) -> None:
    from esphome.components.can_gateway.sensor import CONFIG_SCHEMA

    _validated_gateway(set_core_config)
    CONFIG_SCHEMA(
        {
            "route_id": "route_ab",
            "forwarded": {"name": "fwd"},
            "filtered": {"name": "flt"},
            "tx_full": {"name": "shed"},
            "bus_off": {"name": "dead"},
            "disabled": {"name": "off"},
        }
    )


def test_v08_port_counters_and_gauges_accepted(set_core_config) -> None:
    from esphome.components.can_gateway.sensor import CONFIG_SCHEMA

    _validated_gateway(set_core_config)
    CONFIG_SCHEMA(
        {
            "port_id": "port_a",
            "injected": {"name": "inj"},
            "tx_fail": {"name": "fail"},
            "bus_err": {"name": "err"},
            "recoveries": {"name": "rec"},
            "tec": {"name": "tec"},
            "rec": {"name": "rxe"},
        }
    )


def test_v08_route_and_port_id_together_rejected(set_core_config) -> None:
    from esphome.components.can_gateway.sensor import CONFIG_SCHEMA

    _validated_gateway(set_core_config)
    with pytest.raises(cv.Invalid, match="more than one of route_id, port_id"):
        CONFIG_SCHEMA(
            {"route_id": "route_ab", "port_id": "port_a", "forwarded": {"name": "x"}}
        )


def test_v08_neither_route_nor_port_id_rejected(set_core_config) -> None:
    from esphome.components.can_gateway.sensor import CONFIG_SCHEMA

    _validated_gateway(set_core_config)
    with pytest.raises(cv.Invalid, match="exactly one"):
        CONFIG_SCHEMA({"forwarded": {"name": "x"}})


def test_v08_kind_mismatch_rejected(set_core_config) -> None:
    from esphome.components.can_gateway.sensor import CONFIG_SCHEMA

    _validated_gateway(set_core_config)
    with pytest.raises(cv.Invalid, match="port"):
        CONFIG_SCHEMA({"route_id": "route_ab", "injected": {"name": "mismatch a"}})
    with pytest.raises(cv.Invalid, match="route"):
        CONFIG_SCHEMA({"port_id": "port_a", "forwarded": {"name": "mismatch b"}})


def test_v08_no_subsensor_rejected(set_core_config) -> None:
    from esphome.components.can_gateway.sensor import CONFIG_SCHEMA

    _validated_gateway(set_core_config)
    with pytest.raises(cv.Invalid, match="at least one"):
        CONFIG_SCHEMA({"route_id": "route_ab"})


# ----------------------------------------------------------- binary_sensor


def test_v08_bus_off_binary_sensor_accepted(set_core_config) -> None:
    from esphome.components.can_gateway.binary_sensor import CONFIG_SCHEMA

    _validated_gateway(set_core_config)
    CONFIG_SCHEMA({"port_id": "port_a", "bus_off": {"name": "Bus A bus-off"}})


def test_v08_binary_sensor_requires_subsensor(set_core_config) -> None:
    from esphome.components.can_gateway.binary_sensor import CONFIG_SCHEMA

    _validated_gateway(set_core_config)
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"port_id": "port_a"})


# ------------------------------------------------------------- text_sensor


def test_v08_last_frame_text_sensor_accepted(set_core_config) -> None:
    from esphome.components.can_gateway.text_sensor import CONFIG_SCHEMA

    _validated_gateway(set_core_config)
    validated = CONFIG_SCHEMA(
        {"port_id": "port_a", "last_frame": {"name": "Port A last frame"}}
    )
    assert validated["throttle"].total_milliseconds == 1000  # default 1 s


# ------------------------------------------------------------------ switch


def test_v08_enable_switch_accepted(set_core_config) -> None:
    from esphome.components.can_gateway.switch import CONFIG_SCHEMA

    _validated_gateway(set_core_config)
    validated = CONFIG_SCHEMA({"name": "Gateway enable"})
    # Without explicit restore_mode the gateway boots enabled.
    assert "restore_mode" in validated


def test_v08_second_switch_instance_rejected(
    set_core_config, set_component_config
) -> None:
    from esphome.components.can_gateway import FINAL_VALIDATE_SCHEMA

    config = _validated_gateway(set_core_config)
    set_component_config("can_gateway", config)
    set_component_config(
        "switch",
        [
            {"platform": "can_gateway", "name": "one"},
            {"platform": "can_gateway", "name": "two"},
        ],
    )
    with pytest.raises(cv.Invalid, match="one"):
        FINAL_VALIDATE_SCHEMA(config)
