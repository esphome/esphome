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


def ble_match_bodies(main_cpp: str) -> list[str]:
    """Bodies of the generated BLEAdvertiseDataTrigger match functions, one per trigger."""
    return [
        " ".join(line.strip() for line in body.strip().splitlines())
        for body in re.findall(
            r"BLEAdvertiseDataTrigger\(\w+, \[\]\(const ble_device_base::ESPBTDevice & device\) "
            r"-> const std::vector<uint8_t> \* \{\n(.*?)\n *\}\);",
            main_cpp,
            re.DOTALL,
        )
    ]


def ble_match_body(
    factory: str, getter: str, mac: str | None = None, raw: list[str] | None = None
) -> str:
    """Expected ble_match_bodies() entry; `raw` lists the 128-bit UUID bytes in wire order."""
    parts = []
    if mac is not None:
        parts.append(f"if (device.address_uint64() != {mac}) return nullptr;")
    if raw is not None:
        parts.append(f"static constexpr uint8_t RAW[16] = {{{', '.join(raw)}}};")
    parts.append(
        "static constexpr ble_device_base::ESPBTUUID UUID = "
        f"ble_device_base::ESPBTUUID::{factory};"
    )
    parts.append(f"return ble_device_base::find_adv_data(device.{getter}(), UUID);")
    return " ".join(parts)
