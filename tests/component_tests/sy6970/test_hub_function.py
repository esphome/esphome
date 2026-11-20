"""Tests for the function that publishes the sy6970 child entities."""

from collections.abc import Callable
from pathlib import Path

REG = "esphome__arg_data.registers"
STATUS = f"{REG}[sy6970::SY6970_REG_STATUS]"

# Every child, in config order, with its register decode
EXPECTED_BODY = [
    f"::vbus_connected->publish_state(((({STATUS} >> 5) & 7) != sy6970::BUS_STATUS_NO_INPUT));",
    f"::charging->publish_state(sy6970::is_charging((({STATUS} >> 3) & 3)));",
    f"::charge_done->publish_state(((({STATUS} >> 3) & 3) == sy6970::CHARGE_STATUS_CHARGE_DONE));",
    (
        "::vbus_voltage->publish_state(((sy6970::VBUS_BASE_MV + "
        f"(({REG}[sy6970::SY6970_REG_VBUS_VOLTAGE] & 127) * sy6970::VBUS_STEP_MV)) * 0.001f));"
    ),
    (
        "::battery_voltage->publish_state(((sy6970::VBAT_BASE_MV + "
        f"(({REG}[sy6970::SY6970_REG_BATV] & 127) * sy6970::VBAT_STEP_MV)) * 0.001f));"
    ),
    (
        "::system_voltage->publish_state(((sy6970::VSYS_BASE_MV + "
        f"(({REG}[sy6970::SY6970_REG_VINDPM_STATUS] & 127) * sy6970::VSYS_STEP_MV)) * 0.001f));"
    ),
    (
        "::charge_current->publish_state((("
        f"{REG}[sy6970::SY6970_REG_CHARGE_CURRENT_MONITOR] & 127) * sy6970::CHG_CURRENT_STEP_MA));"
    ),
    (
        "::precharge_current->publish_state((sy6970::PRE_CHG_BASE_MA + "
        f"((({REG}[sy6970::SY6970_REG_PRECHARGE_CURRENT] >> 4) & 15) * sy6970::PRE_CHG_STEP_MA)));"
    ),
    (
        "::bus_status->publish_state(sy6970::BusStatusStrings::get_progmem_str("
        f"(({STATUS} >> 5) & 7), sy6970::BusStatusStrings::LAST_INDEX));"
    ),
    (
        "::charge_status->publish_state(sy6970::ChargeStatusStrings::get_progmem_str("
        f"(({STATUS} >> 3) & 3), sy6970::ChargeStatusStrings::LAST_INDEX));"
    ),
    (
        "::ntc_status->publish_state(sy6970::NtcStatusStrings::get_progmem_str("
        f"({REG}[sy6970::SY6970_REG_FAULT] & 7), sy6970::NtcStatusStrings::LAST_INDEX));"
    ),
]


def test_children_publish_through_hub_function(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))

    head = (
        "static void esphome__charger__data_fn("
        "const sy6970::SY6970Data & esphome__arg_data) {\n"
    )
    body = main_cpp.split(head, 1)[1].split("\n}", 1)[0]
    assert [line.strip() for line in body.splitlines()] == EXPECTED_BODY
    assert "charger->set_data_publisher(esphome__charger__data_fn);" in main_cpp
    # A hub without children keeps a null publisher
    assert "idle_charger->set_data_publisher" not in main_cpp
