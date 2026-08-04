"""Codegen tests for the tracker automations: the generated main is the
automated check on the setter calls and the listener accounting (the
test.ln882x-ard.yaml compile fixture proves linkage, not codegen shape)."""

from esphome.core import CORE


def test_trigger_codegen(generate_main) -> None:
    main_cpp = generate_main(
        "tests/component_tests/ln882h_ble_tracker/test_automations.yaml"
    )

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
    # scan-control actions: templatable continuous lambda + parented actions
    assert "startscanaction_id->set_continuous(" in main_cpp
    assert "stopscanaction_id->set_parent(" in main_cpp
    assert "bleendofscantrigger" in main_cpp.lower()

    # Seven triggers register as listeners; an undercount silently drops the
    # last trigger at runtime (StaticVector::push_back past capacity), so the
    # define is the assertion that matters most.
    defines = {d.name: str(d.value) for d in CORE.defines}
    assert defines.get("ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT") == "7"
