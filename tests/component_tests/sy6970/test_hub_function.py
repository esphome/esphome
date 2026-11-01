"""Tests for the function that publishes the sy6970 child entities."""

from collections.abc import Callable
from pathlib import Path

REG = "esphome__arg_data.registers"


def test_children_publish_through_hub_function(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))

    assert (
        "static void esphome__charger__data_fn("
        "const sy6970::SY6970Data & esphome__arg_data) {"
    ) in main_cpp
    assert "charger->set_data_publisher(esphome__charger__data_fn);" in main_cpp
    # A hub without children keeps a null publisher
    assert "idle_charger->set_data_publisher" not in main_cpp

    assert (
        f"::vbus_connected->publish_state(((({REG}[sy6970::SY6970_REG_STATUS] >> 5) "
        "& 7) != sy6970::BUS_STATUS_NO_INPUT));"
    ) in main_cpp
    assert (
        f"::charging->publish_state(sy6970::is_charging((({REG}"
        "[sy6970::SY6970_REG_STATUS] >> 3) & 3)));"
    ) in main_cpp
    assert (
        "::battery_voltage->publish_state(((sy6970::VBAT_BASE_MV + "
        f"(({REG}[sy6970::SY6970_REG_BATV] & 127) * sy6970::VBAT_STEP_MV)) * 0.001f));"
    ) in main_cpp
    assert (
        "::precharge_current->publish_state((sy6970::PRE_CHG_BASE_MA + "
        f"((({REG}[sy6970::SY6970_REG_PRECHARGE_CURRENT] >> 4) & 15) "
        "* sy6970::PRE_CHG_STEP_MA)));"
    ) in main_cpp
    assert (
        "::ntc_status->publish_state(sy6970::NtcStatusStrings::get_progmem_str("
        f"({REG}[sy6970::SY6970_REG_FAULT] & 7), "
        "sy6970::NtcStatusStrings::LAST_INDEX));"
    ) in main_cpp
    # Unconfigured children add nothing
    assert "charge_done" not in main_cpp
