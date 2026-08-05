"""Schema tests for can_gateway cyclic_sends and their runtime actions.

cfg-v16: a cyclic send transmits a staged payload on a port every `interval`.
Field bounds are enforced by the schema; the port cross-references (declared,
not listen-only) are enforced by the component's _validate_gateway step.
"""

from __future__ import annotations

import pytest

from esphome import config_validation as cv

from .common import PORT_A, PORT_B, gateway, port, setup_c6, validate


def _cyclic(**overrides):
    base = {
        "id": "heartbeat",
        "port": "port_a",
        "can_id": 0x700,
        "interval": "100ms",
    }
    base.update(overrides)
    return base


# ---------------------------------------------------------------- accept


def test_cyclic_basic_accepted(set_core_config) -> None:
    setup_c6(set_core_config)
    validated = validate(gateway(cyclic_sends=[_cyclic(data=[0x01, 0x02])]))
    assert len(validated["cyclic_sends"]) == 1
    assert validated["cyclic_sends"][0]["enabled"] is True


def test_cyclic_defaults(set_core_config) -> None:
    setup_c6(set_core_config)
    validated = validate(gateway(cyclic_sends=[_cyclic()]))
    entry = validated["cyclic_sends"][0]
    assert entry["data"] == []
    assert entry["use_extended_id"] is False
    assert entry["remote_transmission_request"] is False


def test_cyclic_extended_and_rtr_accepted(set_core_config) -> None:
    setup_c6(set_core_config)
    validate(
        gateway(
            cyclic_sends=[
                _cyclic(can_id=0x18DAF110, use_extended_id=True, data=[1, 2, 3]),
                _cyclic(
                    id="poll",
                    port="port_b",
                    can_id=0x123,
                    remote_transmission_request=True,
                ),
            ]
        )
    )


# ---------------------------------------------------------------- reject


def test_cyclic_interval_zero_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid):
        validate(gateway(cyclic_sends=[_cyclic(interval="0ms")]))


def test_cyclic_rtr_with_data_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="RTR"):
        validate(
            gateway(
                cyclic_sends=[
                    _cyclic(remote_transmission_request=True, data=[1]),
                ]
            )
        )


def test_cyclic_standard_id_bound_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="0x7FF"):
        validate(gateway(cyclic_sends=[_cyclic(can_id=0x800)]))


def test_cyclic_nine_bytes_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid):
        validate(gateway(cyclic_sends=[_cyclic(data=list(range(9)))]))


def test_cyclic_undeclared_port_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="not a declared port"):
        validate(gateway(cyclic_sends=[_cyclic(port="port_x")]))


def test_cyclic_listen_only_port_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    ports = [port(PORT_A, listen_only=True), dict(PORT_B)]
    with pytest.raises(cv.Invalid, match="listen-only"):
        validate(gateway(ports=ports, cyclic_sends=[_cyclic(port="port_a")]))
    # A cyclic send on the transmitting port is fine.
    validate(gateway(ports=ports, cyclic_sends=[_cyclic(port="port_b")]))


def test_cyclic_count_capped(set_core_config) -> None:
    # The cap keeps the per-port inject slot pool inside its uint8_t sizing.
    setup_c6(set_core_config)
    sends = [_cyclic(id=f"cyclic_{i}", can_id=0x100 + i) for i in range(33)]
    with pytest.raises(cv.Invalid):
        validate(gateway(cyclic_sends=sends))


# ---------------------------------------------------------------- actions


def _action_schemas():
    from esphome.components.can_gateway import (
        CYCLIC_STATE_ACTION_SCHEMA,
        SET_CYCLIC_DATA_ACTION_SCHEMA,
    )

    return SET_CYCLIC_DATA_ACTION_SCHEMA, CYCLIC_STATE_ACTION_SCHEMA


def test_set_cyclic_data_action_accepted(set_core_config) -> None:
    set_data, _ = _action_schemas()
    setup_c6(set_core_config)
    set_data({"id": "heartbeat", "data": [0x01, 0x02, 0x03]})


def test_set_cyclic_data_nine_bytes_rejected(set_core_config) -> None:
    set_data, _ = _action_schemas()
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid):
        set_data({"id": "heartbeat", "data": list(range(9))})


def test_cyclic_state_actions_accepted(set_core_config) -> None:
    _, state = _action_schemas()
    setup_c6(set_core_config)
    state({"id": "heartbeat"})
