"""The BQ25186 status fields from the datasheet, for checking the entity decode."""

from pathlib import Path
import re

# Offsets into the status registers the hub reads from STAT0
STAT0 = 0
STAT1 = 1
FLAG0 = 2

# Binary sensor key: (register, field bits, field values that read as on), in publish order
BINARY_SENSORS: dict[str, tuple[int, int, frozenset[int]]] = {
    "ts": (STAT0, 0x80, frozenset({1})),
    "charging": (STAT0, 0x60, frozenset({1, 2})),
    "charge_done": (STAT0, 0x60, frozenset({3})),
    "ilim": (STAT0, 0x10, frozenset({1})),
    "vdppm": (STAT0, 0x08, frozenset({1})),
    "vindpm": (STAT0, 0x04, frozenset({1})),
    "thermreg": (STAT0, 0x02, frozenset({1})),
    "power_good": (STAT0, 0x01, frozenset({1})),
    "vin_ovp": (STAT1, 0x80, frozenset({1})),
    "battery_uvlo": (STAT1, 0x40, frozenset({1})),
    "safety_timer_flag": (STAT1, 0x04, frozenset({1})),
    "wake1_flag": (STAT1, 0x02, frozenset({1})),
    "wake2_flag": (STAT1, 0x01, frozenset({1})),
    "ts_flag": (FLAG0, 0x80, frozenset({1})),
    "ilim_flag": (FLAG0, 0x40, frozenset({1})),
    "vdppm_flag": (FLAG0, 0x20, frozenset({1})),
    "vindpm_flag": (FLAG0, 0x10, frozenset({1})),
    "thermreg_flag": (FLAG0, 0x08, frozenset({1})),
    "vin_ovp_flag": (FLAG0, 0x04, frozenset({1})),
    "battery_uvlo_flag": (FLAG0, 0x02, frozenset({1})),
    "battery_ocp_flag": (FLAG0, 0x01, frozenset({1})),
}

# Text sensor key: (register, field bits, the state for each field value), in publish order
TEXT_SENSORS: dict[str, tuple[int, int, tuple[str, ...]]] = {
    "charge_status": (
        STAT0,
        0x60,
        (
            "Not Charging",
            "Constant Current",
            "Constant Voltage",
            "Charge Done/Disabled",
        ),
    ),
    "ts_status": (
        STAT1,
        0x18,
        (
            "Normal",
            "Hot/Cold (Charging Suspended)",
            "Cool (Current Reduced)",
            "Warm (Voltage Reduced)",
        ),
    ),
}


def field_value(value: int, bits: int) -> int:
    """The field `bits` of the register `value`, shifted down."""
    return (value & bits) >> ((bits & -bits).bit_length() - 1)


def status_strings() -> list[str]:
    """The strings of the component's PROGMEM_STRING_TABLE, in table order."""
    component = Path(__file__).parents[3] / "esphome" / "components" / "bq25186"
    for path in sorted(component.glob("*.[ch]*")):
        if match := re.search(
            r"PROGMEM_STRING_TABLE\(\w+,(.*?)\);", path.read_text(), re.DOTALL
        ):
            return re.findall(r'"([^"]*)"', match[1])
    raise AssertionError("no PROGMEM_STRING_TABLE in the bq25186 component")
