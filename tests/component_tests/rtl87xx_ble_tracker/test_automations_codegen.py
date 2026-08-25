"""Codegen tests for the tracker automations.

The shared trigger classes (ble_device_base/automation.h) are compiled by every
esp32 BLE compile test via AUTO_LOAD, and the RTL-specific action templates
compile on the rtl87xxc-ard arm (test.rtl87xxc-ard.yaml uses them from
on_boot). These tests still pin the generated main on an AmebaD (bw16) board,
which no CI arm can compile: the setter spellings, the action templates and the
listener accounting are otherwise unchecked there."""

from collections.abc import Callable
from pathlib import Path
import re

from esphome.components import ble_device_base
from tests.component_tests.helpers import get_define_value


def test_trigger_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("test_automations.yaml"))

    # on_ble_advertise: multi-mac filter (two addresses in one initializer list)
    assert "set_addresses({0xAC3743775F4CULL, 0x112233445566ULL})" in main_cpp
    # 128-bit service uuid goes out reversed (BLE wire order); single-mac filter
    assert (
        "set_service_uuid128((uint8_t*)(const uint8_t[16]){0xCD,0xAB,0xCD,0xAB,"
        "0xCD,0xAB,0xCD,0xAB,0xCD,0xAB,0xCD,0xAB,0xCD,0xAB,0xCD,0xAB})" in main_cpp
    )
    assert "set_address(0xAC3743775F4CULL)" in main_cpp
    # 32-bit middle branch of the width dispatch
    assert "set_service_uuid32(0xABCDABCDULL)" in main_cpp
    # All three manufacturer widths: getattr() builds these names as strings,
    # so a misspelling only ever fails here.
    assert "set_manufacturer_uuid16(0xABCDULL)" in main_cpp
    assert "set_manufacturer_uuid32(0xABCDABCDULL)" in main_cpp
    assert (
        "set_manufacturer_uuid128((uint8_t*)(const uint8_t[16]){0xCD,0xAB,0xCD,0xAB,"
        "0xCD,0xAB,0xCD,0xAB,0xCD,0xAB,0xCD,0xAB,0xCD,0xAB,0xCD,0xAB})" in main_cpp
    )
    # scan-control actions: templatable continuous lambda + parented actions.
    # Exactly one set_continuous: the bare start_scan emits none, pinning the
    # restore-configured-mode divergence from esp32 against a future default=.
    assert main_cpp.count("->set_continuous(") == 1
    assert "startscanaction_id->set_continuous(" in main_cpp
    assert "stopscanaction_id->set_parent(" in main_cpp
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

    # Defaults reach the setters as whole milliseconds.
    assert "->set_scan_interval(100)" in main_cpp
    assert "->set_scan_window(30)" in main_cpp

    # Seven triggers register as listeners; an undercount silently drops the
    # last trigger at runtime (StaticVector::push_back past capacity), so the
    # define is the assertion that matters most.
    assert get_define_value(ble_device_base.LISTENER_COUNT_DEFINE) == "7"


def test_sub_millisecond_values_round_up(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    # set_scan_params() takes whole milliseconds; sub-millisecond values must round UP so
    # the validated 2.5 ms floor cannot truncate below the controller minimum.
    main_cpp = generate_main(component_config_path("test_boundary_ms.yaml"))
    assert "->set_scan_interval(5)" in main_cpp
    assert "->set_scan_window(3)" in main_cpp
