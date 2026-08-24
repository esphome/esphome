"""Connection-slot accounting: consumers claim against MAX_CONNECTIONS and
final validation rejects over-subscription with the consumer list."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components import rp2040_ble
from esphome.core import CORE


def test_proxy_claims_its_slots_through_the_shared_accounting(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    # A default (3-slot) proxy build records one claim per slot, attributed
    # to the consumer, and passes final validation.
    generate_main(component_config_path("rp2_proxy_default.yaml"))
    used = CORE.data[rp2040_ble.KEY_RP2040_BLE][rp2040_ble.KEY_USED_CONNECTION_SLOTS]
    assert used == ["bluetooth_proxy"] * 3


def test_oversubscription_is_rejected_with_the_consumer_list() -> None:
    # No YAML shape reaches this today (the proxy schema caps at the same
    # limit); the guard exists for a second consumer such as ble_client.
    rp2040_ble.consume_connection_slots(3, "bluetooth_proxy")({})
    rp2040_ble.consume_connection_slots(1, "ble_client")({})
    with pytest.raises(
        cv.Invalid,
        match=r"4 connection slots.*maximum is 3.*bluetooth_proxy.*ble_client",
    ):
        rp2040_ble.validate_connection_slots()


def test_at_cap_passes() -> None:
    rp2040_ble.consume_connection_slots(3, "bluetooth_proxy")({})
    rp2040_ble.validate_connection_slots()
