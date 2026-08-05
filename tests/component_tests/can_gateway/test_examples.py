"""Representative full-configuration examples, validated end-to-end.

Each example is a complete, realistic gateway configuration: a bidirectional
bridge, an allow-list firewall, ID translation with a payload bit fix, a
runtime-updatable modification, a listen-only tap, and bus-off/recovery
automations.
"""

from __future__ import annotations

import yaml as pyyaml

from .common import setup_c6

STANDARD_PORTS = """
ports:
  - id: port_a
    rx_pin: GPIO4
    tx_pin: GPIO5
    bit_rate: 50kbps
  - id: port_b
    rx_pin: GPIO6
    tx_pin: GPIO7
    bit_rate: 500kbps
"""

# Two-port bidirectional bridge, forwarding everything both ways.
EXAMPLE_BRIDGE = """
ports:
  - id: port_a
    rx_pin: GPIO4
    tx_pin: GPIO5
    bit_rate: 50kbps
  - id: port_b
    rx_pin: GPIO6
    tx_pin: GPIO7
    bit_rate: 500kbps
routes:
  - from: port_a
    to: port_b
  - from: port_b
    to: port_a
"""

# Allow-list firewall: only the listed IDs cross.
EXAMPLE_FIREWALL = (
    """
ports:
  - id: port_device
    rx_pin: GPIO4
    tx_pin: GPIO5
    bit_rate: 125kbps
  - id: port_main
    rx_pin: GPIO6
    tx_pin: GPIO7
    bit_rate: 125kbps
"""
    """
routes:
  - from: port_device
    to: port_main
    default_action: drop
    filters:
      - can_id: 0x181
      - can_id: 0x281
      - can_id: 0x381
"""
)

# ID translation plus a payload bit fix.
EXAMPLE_TRANSLATION = (
    STANDARD_PORTS
    + """
routes:
  - from: port_a
    to: port_b
    filters:
      - can_id: 0x180
        modify:
          can_id: 0x580
      - can_id: 0x2A0
        modify:
          data:
            - index: 0        # clear the vendor's stuck error bit
              value: 0x00
              mask: 0x80
"""
)

# Runtime-updatable modification: a rule with an `id` whose staged value can
# be changed later via can_gateway.set_patch.
EXAMPLE_LIVE_VALUE = """
ports:
  - id: port_battery
    rx_pin: GPIO4
    tx_pin: GPIO5
    bit_rate: 500kbps
  - id: port_inverter
    rx_pin: GPIO6
    tx_pin: GPIO7
    bit_rate: 500kbps
routes:
  - from: port_battery
    to: port_inverter
    filters:
      - id: charge_limit
        can_id: 0x355
        modify:
          data:
            - index: 2          # declare the patched byte; initial value
              value: 0x64
"""

# Listen-only tap: mirror a live bus onto a bench bus without disturbing it.
EXAMPLE_TAP = """
ports:
  - id: port_live
    rx_pin: GPIO4
    tx_pin: GPIO5      # required by the transceiver, but never driven
    bit_rate: 250kbps
    listen_only: true
  - id: port_bench
    rx_pin: GPIO6
    tx_pin: GPIO7
    bit_rate: 250kbps
routes:
  - from: port_live
    to: port_bench
"""

# Bus-off / recovery automations (light actions replaced with logger.log so
# the test needs no light component; the automation shape is preserved).
EXAMPLE_BEACON = """
ports:
  - id: port_car
    rx_pin: GPIO11
    tx_pin: GPIO10
    bit_rate: 500kbps
    on_bus_off:
      then:
        - logger.log: "bus off"
    on_recovered:
      then:
        - logger.log: "recovered"
  - id: port_other
    rx_pin: GPIO4
    tx_pin: GPIO5
    bit_rate: 500kbps
routes:
  - from: port_car
    to: port_other
"""


def _validate(yaml_text: str):
    from esphome.components.can_gateway import CONFIG_SCHEMA

    return CONFIG_SCHEMA(pyyaml.safe_load(yaml_text))


def test_example_bridge(set_core_config) -> None:
    setup_c6(set_core_config)
    _validate(EXAMPLE_BRIDGE)


def test_example_firewall(set_core_config) -> None:
    setup_c6(set_core_config)
    _validate(EXAMPLE_FIREWALL)


def test_example_translation(set_core_config) -> None:
    setup_c6(set_core_config)
    _validate(EXAMPLE_TRANSLATION)


def test_example_live_value(set_core_config) -> None:
    setup_c6(set_core_config)
    _validate(EXAMPLE_LIVE_VALUE)


def test_example_set_patch_action(set_core_config) -> None:
    from esphome.components.can_gateway import SET_PATCH_ACTION_SCHEMA

    setup_c6(set_core_config)
    # The set_patch action targeting the live-value rule above, with a static
    # value standing in for the lambda (templatable either way).
    SET_PATCH_ACTION_SCHEMA(
        {"id": "charge_limit", "data": [{"index": 2, "value": 0x42}]}
    )


def test_example_tap(set_core_config) -> None:
    setup_c6(set_core_config)
    _validate(EXAMPLE_TAP)


def test_example_beacon(set_core_config) -> None:
    setup_c6(set_core_config)
    _validate(EXAMPLE_BEACON)


def test_example_inject_action(set_core_config) -> None:
    from esphome.components.can_gateway import INJECT_ACTION_SCHEMA

    setup_c6(set_core_config)
    INJECT_ACTION_SCHEMA(
        {"port": "port_a", "can_id": 0x100, "data": [0x01, 0x02, 0x03]}
    )
