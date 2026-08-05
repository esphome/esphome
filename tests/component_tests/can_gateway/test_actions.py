"""Schema tests for can_gateway.set_patch (cfg-v10) and can_gateway.inject (cfg-v11).

Inline bounds are enforced by the action schemas; cross-references against the
gateway block (declared patch shape, listen-only ports) are enforced by the
component's final-validate step, which scans the full config for gateway
actions.
"""

from __future__ import annotations

import pytest

from esphome import config_validation as cv

from .common import PORT_A, PORT_B, gateway, port, route, setup_c6


def _schemas():
    from esphome.components.can_gateway import (
        INJECT_ACTION_SCHEMA,
        SET_PATCH_ACTION_SCHEMA,
    )

    return SET_PATCH_ACTION_SCHEMA, INJECT_ACTION_SCHEMA


def _final_validate(gateway_config, actions, set_component_config):
    """Run the component final-validate with `actions` planted in the full config."""
    from esphome.components.can_gateway import FINAL_VALIDATE_SCHEMA

    set_component_config("can_gateway", gateway_config)
    # Where the automation lives is irrelevant; the scan is recursive.
    set_component_config(
        "button",
        [{"platform": "template", "on_press": [{"then": actions}]}],
    )
    return FINAL_VALIDATE_SCHEMA(gateway_config)


def _patch_rule_gateway(set_core_config, *, with_can_id: bool = False):
    """A validated gateway whose route declares updatable rule `charge_limit`."""
    from esphome.components.can_gateway import CONFIG_SCHEMA

    setup_c6(set_core_config)
    modify = {"data": [{"index": 2, "value": 0x64}]}
    if with_can_id:
        modify["can_id"] = 0x581
    return CONFIG_SCHEMA(
        gateway(
            routes=[
                route(
                    filters=[{"id": "charge_limit", "can_id": 0x355, "modify": modify}]
                )
            ]
        )
    )


# ---------------------------------------------------------------- cfg-v10


def test_v10_declared_byte_accepted(set_core_config, set_component_config) -> None:
    set_patch_schema, _ = _schemas()
    config = _patch_rule_gateway(set_core_config)
    action = set_patch_schema(
        {"id": "charge_limit", "data": [{"index": 2, "value": 0x42}]}
    )
    _final_validate(config, [{"can_gateway.set_patch": action}], set_component_config)


def test_v10_undeclared_byte_rejected(set_core_config, set_component_config) -> None:
    set_patch_schema, _ = _schemas()
    config = _patch_rule_gateway(set_core_config)
    action = set_patch_schema(
        {"id": "charge_limit", "data": [{"index": 3, "value": 0x42}]}
    )
    with pytest.raises(cv.Invalid, match="declare"):
        _final_validate(
            config, [{"can_gateway.set_patch": action}], set_component_config
        )


def test_v10_can_id_only_when_declared(set_core_config, set_component_config) -> None:
    set_patch_schema, _ = _schemas()
    config = _patch_rule_gateway(set_core_config)
    action = set_patch_schema({"id": "charge_limit", "can_id": 0x582})
    with pytest.raises(cv.Invalid, match="can_id"):
        _final_validate(
            config, [{"can_gateway.set_patch": action}], set_component_config
        )


def test_v10_declared_can_id_accepted(set_core_config, set_component_config) -> None:
    set_patch_schema, _ = _schemas()
    config = _patch_rule_gateway(set_core_config, with_can_id=True)
    action = set_patch_schema({"id": "charge_limit", "can_id": 0x582})
    _final_validate(config, [{"can_gateway.set_patch": action}], set_component_config)


def test_v10_can_id_outside_output_frame_type_rejected(
    set_core_config, set_component_config
) -> None:
    # The rule's output frame type is standard (11-bit); a static 29-bit
    # value would be silently truncated on the wire, so it must be rejected.
    set_patch_schema, _ = _schemas()
    config = _patch_rule_gateway(set_core_config, with_can_id=True)
    action = set_patch_schema({"id": "charge_limit", "can_id": 0x18DAF110})
    with pytest.raises(cv.Invalid, match="0x7FF"):
        _final_validate(
            config, [{"can_gateway.set_patch": action}], set_component_config
        )


def test_v10_unknown_rule_rejected(set_core_config, set_component_config) -> None:
    set_patch_schema, _ = _schemas()
    config = _patch_rule_gateway(set_core_config)
    action = set_patch_schema(
        {"id": "other_rule", "data": [{"index": 2, "value": 0x42}]}
    )
    with pytest.raises(cv.Invalid, match="filter rule"):
        _final_validate(
            config, [{"can_gateway.set_patch": action}], set_component_config
        )


def test_v10_empty_action_rejected(set_core_config) -> None:
    set_patch_schema, _ = _schemas()
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid):
        set_patch_schema({"id": "charge_limit"})


# ---------------------------------------------------------------- cfg-v11


def test_v11_inject_accepted(set_core_config) -> None:
    _, inject_schema = _schemas()
    setup_c6(set_core_config)
    inject_schema({"port": "port_a", "can_id": 0x100, "data": [1, 2, 3]})
    inject_schema({"port": "port_a", "can_id": 0x100})  # no data = DLC 0
    inject_schema(
        {
            "port": "port_a",
            "can_id": 0x18DAF110,
            "use_extended_id": True,
            "data": [1, 2, 3, 4, 5, 6, 7, 8],
        }
    )
    inject_schema(
        {"port": "port_a", "can_id": 0x100, "remote_transmission_request": True}
    )


def test_v11_nine_bytes_rejected(set_core_config) -> None:
    _, inject_schema = _schemas()
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid):
        inject_schema({"port": "port_a", "can_id": 0x100, "data": list(range(9))})


def test_v11_rtr_with_data_rejected(set_core_config) -> None:
    _, inject_schema = _schemas()
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="RTR"):
        inject_schema(
            {
                "port": "port_a",
                "can_id": 0x100,
                "remote_transmission_request": True,
                "data": [1],
            }
        )


def test_v11_standard_id_bound_rejected(set_core_config) -> None:
    _, inject_schema = _schemas()
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="0x7FF"):
        inject_schema({"port": "port_a", "can_id": 0x800})


def test_v11_inject_on_listen_only_rejected(
    set_core_config, set_component_config
) -> None:
    from esphome.components.can_gateway import CONFIG_SCHEMA

    _, inject_schema = _schemas()
    setup_c6(set_core_config)
    config = CONFIG_SCHEMA(
        gateway(
            ports=[port(PORT_A, listen_only=True), dict(PORT_B)],
            routes=[{"from": "port_a", "to": "port_b"}],
        )
    )
    action = inject_schema({"port": "port_a", "can_id": 0x100})
    with pytest.raises(cv.Invalid, match="listen.only"):
        _final_validate(config, [{"can_gateway.inject": action}], set_component_config)
    # Injecting on the normal port is fine.
    action = inject_schema({"port": "port_b", "can_id": 0x100})
    _final_validate(config, [{"can_gateway.inject": action}], set_component_config)
