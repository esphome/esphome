"""Schema tests for the can_gateway component block.

Covers cfg-v01 (ports/routes), cfg-v02…cfg-v06 (filter rules), cfg-v07
(listen-only routes), cfg-v09 (bit rate), cfg-v12 (bench-aid exclusivity).
"""

from __future__ import annotations

import pytest

from esphome import config_validation as cv

from .common import PORT_A, PORT_B, gateway, port, route, setup_c6, validate

# ---------------------------------------------------------------- cfg-v01


def test_v01_two_ports_one_route_accepted(set_core_config) -> None:
    setup_c6(set_core_config)
    validated = validate(gateway())
    assert len(validated["ports"]) == 2
    assert len(validated["routes"]) == 1


def test_v01_bidirectional_routes_accepted(set_core_config) -> None:
    setup_c6(set_core_config)
    validated = validate(gateway(routes=[route(), {"from": "port_b", "to": "port_a"}]))
    assert len(validated["routes"]) == 2


def test_v01_one_port_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="exactly 2"):
        validate(gateway(ports=[dict(PORT_A)]))


def test_v01_three_ports_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    third = port(PORT_A, id="port_c", rx_pin="GPIO5", tx_pin="GPIO4")
    with pytest.raises(cv.Invalid, match="exactly 2"):
        validate(gateway(ports=[dict(PORT_A), dict(PORT_B), third]))


def test_v01_route_from_equals_to_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="must differ"):
        validate(gateway(routes=[{"from": "port_a", "to": "port_a"}]))


def test_v01_route_to_undeclared_port_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="not a declared port"):
        validate(gateway(routes=[{"from": "port_a", "to": "port_x"}]))


def test_v01_no_routes_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid):
        validate(gateway(routes=[]))


def test_v01_duplicate_direction_rejected(set_core_config) -> None:
    # The fast path receives each frame exactly once, straight into the single
    # outbound route's TX slot (N4); a second route from the same port would
    # need a second copy.
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="one route per direction"):
        validate(gateway(routes=[route(), route(default_action="drop")]))


# ---------------------------------------------------------------- cfg-v02


def test_v02_standard_id_bounds(set_core_config) -> None:
    setup_c6(set_core_config)
    validate(gateway(routes=[route(filters=[{"can_id": 0x7FF}])]))
    with pytest.raises(cv.Invalid, match="0x7FF"):
        validate(gateway(routes=[route(filters=[{"can_id": 0x800}])]))


def test_v02_extended_id_bounds(set_core_config) -> None:
    setup_c6(set_core_config)
    validate(
        gateway(
            routes=[route(filters=[{"can_id": 0x1FFFFFFF, "use_extended_id": True}])]
        )
    )
    with pytest.raises(cv.Invalid):
        validate(
            gateway(
                routes=[
                    route(filters=[{"can_id": 0x2000_0000, "use_extended_id": True}])
                ]
            )
        )


def test_v02_modify_id_checked_against_output_type(set_core_config) -> None:
    setup_c6(set_core_config)
    # Standard in, standard out: new ID must fit 11 bits.
    with pytest.raises(cv.Invalid, match="0x7FF"):
        validate(
            gateway(
                routes=[route(filters=[{"can_id": 0x100, "modify": {"can_id": 0x800}}])]
            )
        )
    # Standard in, promoted to extended out: 29-bit ID is fine.
    validate(
        gateway(
            routes=[
                route(
                    filters=[
                        {
                            "can_id": 0x100,
                            "modify": {
                                "can_id": 0x18DAF110,
                                "use_extended_id": True,
                            },
                        }
                    ]
                )
            ]
        )
    )


def test_v02_demotion_to_standard_requires_explicit_id(set_core_config) -> None:
    # Extended -> standard without a new can_id could carry a 29-bit ID into
    # an 11-bit frame; the schema requires an explicit ID for demotion.
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="can_id"):
        validate(
            gateway(
                routes=[
                    route(
                        filters=[
                            {
                                "can_id": 0x18DAF110,
                                "use_extended_id": True,
                                "modify": {"use_extended_id": False},
                            }
                        ]
                    )
                ]
            )
        )


def test_v02_type_change_on_masked_rule_requires_explicit_id(set_core_config) -> None:
    # A masked rule matches a whole ID range; changing the frame type without
    # a new ID would collapse that range onto the rule's can_id.
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="masked rule"):
        validate(
            gateway(
                routes=[
                    route(
                        filters=[
                            {
                                "can_id": 0x300,
                                "can_id_mask": 0x700,
                                "modify": {"use_extended_id": True},
                            }
                        ]
                    )
                ]
            )
        )
    # Exact-match promotion needs nothing: the matched ID is unique.
    validate(
        gateway(
            routes=[
                route(filters=[{"can_id": 0x300, "modify": {"use_extended_id": True}}])
            ]
        )
    )


# ---------------------------------------------------------------- cfg-v03


def test_v03_mask_defaults_and_bounds(set_core_config) -> None:
    setup_c6(set_core_config)
    validated = validate(gateway(routes=[route(filters=[{"can_id": 0x100}])]))
    rule = validated["routes"][0]["filters"][0]
    assert rule["can_id_mask"] == 0x7FF

    validated = validate(
        gateway(routes=[route(filters=[{"can_id": 0x100, "use_extended_id": True}])])
    )
    rule = validated["routes"][0]["filters"][0]
    assert rule["can_id_mask"] == 0x1FFFFFFF

    with pytest.raises(cv.Invalid, match="mask"):
        validate(
            gateway(routes=[route(filters=[{"can_id": 0x100, "can_id_mask": 0x800}])])
        )


# ---------------------------------------------------------------- cfg-v04


def test_v04_id_bits_outside_mask_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    validate(gateway(routes=[route(filters=[{"can_id": 0x100, "can_id_mask": 0x700}])]))
    with pytest.raises(cv.Invalid, match="outside"):
        validate(
            gateway(routes=[route(filters=[{"can_id": 0x123, "can_id_mask": 0x700}])])
        )


# ---------------------------------------------------------------- cfg-v05


def test_v05_modify_with_drop_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    validate(gateway(routes=[route(filters=[{"can_id": 0x100, "action": "drop"}])]))
    with pytest.raises(cv.Invalid, match="drop"):
        validate(
            gateway(
                routes=[
                    route(
                        filters=[
                            {
                                "can_id": 0x100,
                                "action": "drop",
                                "modify": {"can_id": 0x200},
                            }
                        ]
                    )
                ]
            )
        )


# ---------------------------------------------------------------- cfg-v06


def test_v06_patch_bytes_accepted(set_core_config) -> None:
    setup_c6(set_core_config)
    validated = validate(
        gateway(
            routes=[
                route(
                    filters=[
                        {
                            "can_id": 0x100,
                            "modify": {
                                "data": [
                                    {"index": 0, "value": 0x01},
                                    {"index": 7, "value": 0xFF, "mask": 0x80},
                                ]
                            },
                        }
                    ]
                )
            ]
        )
    )
    patches = validated["routes"][0]["filters"][0]["modify"]["data"]
    assert patches[0]["mask"] == 0xFF  # default mask


@pytest.mark.parametrize(
    "patch",
    [
        {"index": 8, "value": 0x01},
        {"index": 0, "value": 0x100},
        {"index": 0, "value": 0x01, "mask": 0x100},
    ],
)
def test_v06_patch_byte_bounds_rejected(set_core_config, patch) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid):
        validate(
            gateway(
                routes=[route(filters=[{"can_id": 0x100, "modify": {"data": [patch]}}])]
            )
        )


def test_v06_duplicate_patch_index_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid, match="once"):
        validate(
            gateway(
                routes=[
                    route(
                        filters=[
                            {
                                "can_id": 0x100,
                                "modify": {
                                    "data": [
                                        {"index": 2, "value": 0x01},
                                        {"index": 2, "value": 0x02},
                                    ]
                                },
                            }
                        ]
                    )
                ]
            )
        )


# ---------------------------------------------------------------- cfg-v07


def test_v07_route_to_listen_only_rejected(set_core_config) -> None:
    setup_c6(set_core_config)
    ports = [port(PORT_A, listen_only=True), dict(PORT_B)]
    # Tap direction (from the listen-only port) is fine.
    validate(gateway(ports=ports))
    with pytest.raises(cv.Invalid, match="listen.only"):
        validate(gateway(ports=ports, routes=[{"from": "port_b", "to": "port_a"}]))


# ---------------------------------------------------------------- cfg-v09


@pytest.mark.parametrize("bit_rate", ["10kbps", "125kbps", "1Mbps", 500000, "500000"])
def test_v09_bit_rate_accepted(set_core_config, bit_rate) -> None:
    setup_c6(set_core_config)
    ports = [port(PORT_A, bit_rate=bit_rate), dict(PORT_B)]
    validate(gateway(ports=ports))


@pytest.mark.parametrize("bit_rate", ["9kbps", 9999, 2_000_000, "fast", "125kHz"])
def test_v09_bit_rate_rejected(set_core_config, bit_rate) -> None:
    setup_c6(set_core_config)
    ports = [port(PORT_A, bit_rate=bit_rate), dict(PORT_B)]
    with pytest.raises(cv.Invalid):
        validate(gateway(ports=ports))


# ---------------------------------------------------------------- cfg-v12


def test_v12_self_test_and_listen_only_exclusive(set_core_config) -> None:
    setup_c6(set_core_config)
    validate(gateway(ports=[port(PORT_A, self_test=True), dict(PORT_B)]))
    validate(gateway(ports=[port(PORT_A, listen_only=True), dict(PORT_B)]))
    with pytest.raises(cv.Invalid, match="mutually exclusive"):
        validate(
            gateway(
                ports=[port(PORT_A, self_test=True, listen_only=True), dict(PORT_B)]
            )
        )


def test_v12_loopback_not_offered(set_core_config) -> None:
    # The C6 TWAI core has no loopback mode (twai_ll_set_mode ignores the
    # flag), so the component must not offer a silently-dead option.
    setup_c6(set_core_config)
    with pytest.raises(cv.Invalid):
        validate(gateway(ports=[port(PORT_A, loopback=True), dict(PORT_B)]))


# ---------------------------------------------------------------- cfg-v15


def test_v15_rule_id_requires_modify(set_core_config) -> None:
    setup_c6(set_core_config)
    # id + modify: the runtime-updatable staple, accepted.
    validate(
        gateway(
            routes=[
                route(
                    filters=[
                        {
                            "id": "limit_rule",
                            "can_id": 0x355,
                            "modify": {"data": [{"index": 2, "value": 0x64}]},
                        }
                    ]
                )
            ]
        )
    )
    # id without modify: nothing set_patch could ever target.
    with pytest.raises(cv.Invalid, match="without a modify block"):
        validate(
            gateway(routes=[route(filters=[{"id": "limit_rule", "can_id": 0x355}])])
        )
