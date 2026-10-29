"""Shared helpers for component tests."""

from __future__ import annotations

import re

INTERNAL_BIT = 1 << 24


def extract_packed_value(main_cpp: str, var_name: str) -> int:
    """Extract the packed-fields argument from the entity's configure call.

    Matches both legacy form ``var->configure_entity_(name, hash, packed)`` and the
    combined form ``App.register_<entity>(var, name, hash, packed)``.
    """
    escaped_var = re.escape(var_name)
    name_re = r'ESPHOME_PSTR\("(?:\\.|[^"\\])*"\)'
    legacy_pattern = (
        rf"{escaped_var}->configure_entity_\(" + name_re + r",\s*\w+,\s*(\d+)\)"
    )
    combined_pattern = (
        rf"App\.register_\w+\(\s*{escaped_var}\s*,\s*" + name_re + r",\s*\w+,\s*(\d+)\)"
    )
    match = re.search(combined_pattern, main_cpp) or re.search(legacy_pattern, main_cpp)
    assert match, f"configure call not found for {var_name}"
    return int(match.group(1))


def get_define_value(name: str) -> str | None:
    """Rendered value of a CORE define, or None when absent.

    Values are codegen expressions (IntLiteral); they are compared rendered.
    A value-less define (e.g. USE_BK72XX_BLE) is present but renders as the
    string "None", while an absent define returns the None object — easy to
    conflate in assertions, so use this helper for valued defines only.
    """
    from esphome.core import CORE

    for define in CORE.defines:
        if define.name == name:
            return str(define.value)
    return None


# 128-bit UUID ABCDABCD-ABCD-ABCD-ABCD-ABCDABCDABCD in BLE wire order, as the test configs use it.
BLE_UUID128_WIRE = ["0xCD", "0xAB"] * 8


def ble_adv_matches(main_cpp: str) -> list[str]:
    """AdvDataMatch initializer of each generated BLEAdvertiseDataTrigger, in config order."""
    constants = dict(
        re.findall(
            r"static constexpr ble_device_base::AdvDataMatch (\w+)\[\] PROGMEM = "
            r"\{ble_device_base::AdvDataMatch\{\n(.*?)\n\}\};",
            main_cpp,
            re.DOTALL,
        )
    )
    return [
        " ".join(line.strip() for line in constants[name].splitlines())
        for name in re.findall(r"BLEAdvertiseDataTrigger\(\w+, (\w+)\);", main_cpp)
    ]


def ble_adv_match(
    factory: str = "",
    *,
    manufacturer: bool = False,
    mac: str | None = None,
    raw: bool = False,
) -> str:
    """Expected ble_adv_matches() entry; `raw` matches the BLE_UUID128_WIRE UUID."""
    if raw:
        factory = (
            f"from_raw(std::array<uint8_t, 16>{{{', '.join(BLE_UUID128_WIRE)}}}.data())"
        )
    return (
        f".address = {mac or 0}, "
        f".uuid = ble_device_base::ESPBTUUID::{factory}, "
        f".manufacturer = {'true' if manufacturer else 'false'}, "
        f".has_address = {'false' if mac is None else 'true'},"
    )
