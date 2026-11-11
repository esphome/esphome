"""Codegen tests for the tracker automations.

The shared trigger classes (ble_device_base/automation.h) are compiled by every
esp32 BLE compile test via AUTO_LOAD, but the BK-specific side — automation.h's
action templates and restart_scan_duration() — compiles on no CI board (the
bk72xx base board generic-bk7252 is BLE 4.2 and cannot build the tracker), and
validate fixtures never run to_code. The generated main is therefore the only
automated check on the setter spellings and the listener accounting."""

from collections.abc import Callable
from pathlib import Path
import re

from esphome.components import ble_device_base
from tests.component_tests.helpers import ble_adv_matches, get_define_value


def test_trigger_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("test_automations.yaml"))

    # on_ble_advertise: multi-mac filter (two addresses in one initializer list)
    assert (
        "static constexpr uint64_t ble_mac_filter[] PROGMEM = "
        "{0xAC3743775F4CULL, 0x112233445566ULL, 0};" in main_cpp
    )
    assert "set_addresses(ble_mac_filter)" in main_cpp
    # Service/manufacturer data triggers come from the shared ble_device_base
    # codegen (full matrix in the esp32_ble_tracker test); check it ran here.
    assert len(ble_adv_matches(main_cpp)) == 5
    # scan-control actions: templatable continuous lambda + stop_scan forwarded
    # straight to the tracker (register_apply_action, no action class).
    # Exactly one set_continuous: the bare start_scan emits none, pinning the
    # restore-configured-mode divergence from esp32 against a future default=.
    assert main_cpp.count("->set_continuous(") == 1
    assert "startscanaction_id->set_continuous(" in main_cpp
    assert main_cpp.count("->stop_scan();") == 1
    # scan_parameters continuous: false reaches the YAML-mode setter, not the
    # runtime override.
    assert "->set_configured_continuous(false)" in main_cpp
    # active: false (non-default) flows through to the setter.
    assert "->set_scan_active(false)" in main_cpp
    # Constructor call, not just the declaration: the parent argument is what
    # registers the trigger as a listener.
    assert re.search(
        r"new\(\w+\) ble_device_base::BLEEndOfScanTrigger\(\w+\)", main_cpp
    )

    # Seven triggers register as listeners; an undercount silently drops the
    # last trigger at runtime (StaticVector::push_back past capacity), so the
    # define is the assertion that matters most.
    assert get_define_value(ble_device_base.LISTENER_COUNT_DEFINE) == "7"
