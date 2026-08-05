"""Schema tests for the can_gateway statistics block and bus_load sensor."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv

from .common import gateway, setup_c6, validate

# ---------------------------------------------------------------- statistics block


def test_statistics_defaults(set_core_config) -> None:
    setup_c6(set_core_config)
    validated = validate(gateway(statistics={}))
    stats = validated["statistics"]
    assert stats["log_interval"].total_milliseconds == 60_000
    assert stats["id_timings"] is False
    assert stats["id_timings_max"] == 32


def test_statistics_id_timings(set_core_config) -> None:
    setup_c6(set_core_config)
    validated = validate(
        gateway(
            statistics={"log_interval": "10s", "id_timings": True, "id_timings_max": 64}
        )
    )
    stats = validated["statistics"]
    assert stats["log_interval"].total_milliseconds == 10_000
    assert stats["id_timings"] is True
    assert stats["id_timings_max"] == 64


def test_statistics_log_disabled_accepted(set_core_config) -> None:
    # 0 leaves only the dump_config summary; the block is still valid.
    setup_c6(set_core_config)
    validate(gateway(statistics={"log_interval": "0s"}))


@pytest.mark.parametrize("max_ids", [0, 129])
def test_statistics_id_timings_max_bounds_rejected(set_core_config, max_ids) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid):
        validate(gateway(statistics={"id_timings_max": max_ids}))


# ---------------------------------------------------------------- bus_load sensor


def test_bus_load_sensor_accepted(set_core_config) -> None:
    from esphome.components.can_gateway.sensor import CONFIG_SCHEMA as SENSOR_SCHEMA

    setup_c6(set_core_config)
    SENSOR_SCHEMA({"port_id": "port_a", "bus_load": {"name": "Bus A load"}})


def test_bus_load_is_a_port_sensor(set_core_config) -> None:
    # bus_load belongs to a port; using it with route_id is a config error.
    from esphome.components.can_gateway.sensor import CONFIG_SCHEMA as SENSOR_SCHEMA

    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="port_id"):
        SENSOR_SCHEMA({"route_id": "route_ab", "bus_load": {"name": "load"}})


def test_bus_load_requires_route_source_port(
    set_core_config, set_component_config
) -> None:
    # A port that is not any route's source never receives through the
    # gateway, so its bus_load gauge would only show the own-TX share.
    from esphome.components.can_gateway import FINAL_VALIDATE_SCHEMA

    setup_c6(set_core_config)
    # One-directional gateway: port_a is the only source.
    config = validate(gateway(routes=[{"from": "port_a", "to": "port_b"}]))
    set_component_config("can_gateway", config)
    set_component_config(
        "sensor",
        [
            {
                "platform": "can_gateway",
                "port_id": "port_b",
                "bus_load": {"name": "load"},
            }
        ],
    )
    with pytest.raises(cv.Invalid, match="route source"):
        FINAL_VALIDATE_SCHEMA(config)


def test_bus_load_on_source_port_accepted(
    set_core_config, set_component_config
) -> None:
    from esphome.components.can_gateway import FINAL_VALIDATE_SCHEMA

    setup_c6(set_core_config)
    config = validate(gateway(routes=[{"from": "port_a", "to": "port_b"}]))
    set_component_config("can_gateway", config)
    set_component_config(
        "sensor",
        [
            {
                "platform": "can_gateway",
                "port_id": "port_a",
                "bus_load": {"name": "load"},
            }
        ],
    )
    FINAL_VALIDATE_SCHEMA(config)
