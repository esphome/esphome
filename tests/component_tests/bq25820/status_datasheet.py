"""The BQ25820 status fields from the datasheet, for checking the entity decode."""

# Offsets into the status registers the hub reads from CHARGER_STATUS_1 (0x21)
CHARGER_STATUS_1 = 0
CHARGER_STATUS_2 = 1
CHARGER_STATUS_3 = 2
FAULT_STATUS = 3

# Binary sensor key: (register, field bits, field values that read as on), in publish order
BINARY_SENSORS: dict[str, tuple[int, int, frozenset[int]]] = {
    # CHARGE_STAT: trickle, precharge, fast, taper, reserved (101b) and top-off
    "charging": (CHARGER_STATUS_1, 0x07, frozenset({1, 2, 3, 4, 5, 6})),
    "charged": (CHARGER_STATUS_1, 0x07, frozenset({7})),
    "low_battery": (CHARGER_STATUS_1, 0x07, frozenset({1, 2})),
    "pg": (CHARGER_STATUS_2, 0x80, frozenset({1})),
    "reverse": (CHARGER_STATUS_3, 0x04, frozenset({1})),
    "vac_undervoltage": (FAULT_STATUS, 0x80, frozenset({1})),
    "vac_overvoltage": (FAULT_STATUS, 0x40, frozenset({1})),
    "ibat_overcurrent": (FAULT_STATUS, 0x20, frozenset({1})),
    "vbat_overvoltage": (FAULT_STATUS, 0x10, frozenset({1})),
    "thermal_shutdown": (FAULT_STATUS, 0x08, frozenset({1})),
    "charge_safety_timer": (FAULT_STATUS, 0x04, frozenset({1})),
    "drv_supply_invalid": (FAULT_STATUS, 0x02, frozenset({1})),
}

# Text sensor key: (register, field bits, the state for each field value), in publish order
TEXT_SENSORS: dict[str, tuple[int, int, tuple[str, ...]]] = {
    "charge_status": (
        CHARGER_STATUS_1,
        0x07,
        (
            "Not Charging",
            "Trickle",
            "Precharge",
            "Fast Charge",
            "Taper",
            "Unknown",
            "Top-Off",
            "Done",
        ),
    ),
    "temp_status": (
        CHARGER_STATUS_2,
        0x70,
        ("Normal", "Warm", "Cool", "Cold", "Hot", "Unknown", "Unknown", "Unknown"),
    ),
}
