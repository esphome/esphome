"""The rp2 BTstack pool overrides: multi-slot builds emit the --wrap flags
that swap the prebuilt single-client pools for the codegen-sized ones;
single-slot builds emit none and stay byte-identical to previous releases."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE

from ..helpers import get_define_value

# Spelled out rather than derived from rp2040_ble's symbol tuple, so a typo
# in the component's list fails here instead of mirroring into the test.
WRAP_FLAGS = (
    "-Wl,--wrap=btstack_memory_gatt_client_get",
    "-Wl,--wrap=btstack_memory_gatt_client_free",
    "-Wl,--wrap=btstack_memory_hci_connection_get",
    "-Wl,--wrap=btstack_memory_hci_connection_free",
)


def test_default_slots_emit_the_pool_wrap(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("rp2_proxy_default.yaml"))
    assert all(flag in CORE.build_flags for flag in WRAP_FLAGS)
    assert get_define_value("ESPHOME_BLE_GATT_CLIENT_COUNT") == "3"
    assert get_define_value("BLUETOOTH_PROXY_MAX_CONNECTIONS") == "3"


def test_two_slots_emit_the_pool_wrap(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    # Two slots: the wrap pools are smaller than the cap, sized from the count.
    generate_main(component_config_path("rp2_proxy_two_slots.yaml"))
    assert all(flag in CORE.build_flags for flag in WRAP_FLAGS)
    assert get_define_value("ESPHOME_BLE_GATT_CLIENT_COUNT") == "2"
    assert get_define_value("BLUETOOTH_PROXY_MAX_CONNECTIONS") == "2"


def test_single_slot_keeps_the_prebuilt_pools(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("rp2_proxy_single_slot.yaml"))
    assert not any(flag in CORE.build_flags for flag in WRAP_FLAGS)
    assert get_define_value("ESPHOME_BLE_GATT_CLIENT_COUNT") == "1"
    assert get_define_value("BLUETOOTH_PROXY_MAX_CONNECTIONS") == "1"
