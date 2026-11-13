"""Enumerated settings for the qualified DeltaSol CS Plus 1.11 profile."""

from typing import NamedTuple


class SelectSpec(NamedTuple):
    index: int
    minimum: int
    labels: tuple[str, ...]


SELECT_PARAMETERS = {
    "system_scheme": SelectSpec(
        1, 1, ("1", "2", "3", "4", "5", "6", "7", "8", "9", "10")
    ),
    "temperature_unit": SelectSpec(10, 0, ("°C", "°F")),
    "storage_priority": SelectSpec(
        21,
        0,
        (
            "Spread loading 1",
            "Spread loading 2",
            "Sequential loading 1",
            "Sequential loading 2",
            "Parallel loading",
            "Storage 1 priority",
            "Storage 2 priority",
        ),
    ),
    "display_language": SelectSpec(
        55, 1, ("German", "English", "French", "Italian", "Spanish")
    ),
    "holiday_cooling": SelectSpec(64, 0, ("Off", "On")),
    "storage_cooling": SelectSpec(71, 0, ("Off", "On")),
    "flow_sensor_type": SelectSpec(74, 0, ("Off", "12", "40", "40F")),
    "heat_meter": SelectSpec(81, 0, ("Off", "On")),
    "flow_sensor_position": SelectSpec(
        85, 0, ("Fixed flow", "Flow sensor", "Return sensor")
    ),
    "heat_transfer_fluid": SelectSpec(
        90, 0, ("Water", "Propylene glycol", "Ethylene glycol", "Tyfocor LS / G-LS")
    ),
    "collector_cooling_1": SelectSpec(461, 0, ("Off", "On")),
    "collector_cooling_2": SelectSpec(462, 0, ("Off", "On")),
    "collector_minimum_limit_1": SelectSpec(512, 0, ("Off", "On")),
    "collector_minimum_limit_2": SelectSpec(513, 0, ("Off", "On")),
    "collector_antifreeze_1": SelectSpec(544, 0, ("Off", "On")),
    "collector_antifreeze_2": SelectSpec(545, 0, ("Off", "On")),
    "relay_mode_1": SelectSpec(552, 0, ("Off", "Auto", "On")),
    "relay_mode_2": SelectSpec(553, 0, ("Off", "Auto", "On")),
    "disable_storage_safety_shutdown": SelectSpec(560, 0, ("Off", "On")),
    "thermal_disinfection": SelectSpec(2560, 0, ("Off", "On")),
    "system_cooling": SelectSpec(2569, 0, ("Off", "On")),
    "drainback": SelectSpec(2768, 0, ("Off", "On")),
    "drainback_booster": SelectSpec(2775, 0, ("Off", "On")),
    "tube_collector_function": SelectSpec(3897, 0, ("Off", "On")),
    "pump_control_1": SelectSpec(12549, 0, ("OnOF", "PULS", "PSOL", "PRIS")),
    "pump_control_2": SelectSpec(12550, 0, ("OnOF", "PULS", "PSOL", "PRIS")),
}
