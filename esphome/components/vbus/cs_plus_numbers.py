"""Numeric setting metadata for the qualified DeltaSol CS Plus 1.11 profile."""

from typing import NamedTuple


class NumberSpec(NamedTuple):
    index: int
    minimum: int
    maximum: int
    step: int
    scale: float
    unit: str = ""


NUMBER_PARAMETERS = {
    "system_time": NumberSpec(2, 0, 1439, 1, 1.0, "min"),
    "loading_break_time": NumberSpec(22, 0, 30, 1, 1.0, "min"),
    "loading_run_time": NumberSpec(23, 1, 30, 1, 1.0, "min"),
    "min_pump_speed_1": NumberSpec(48, 10, 100, 5, 1.0, "%"),
    "min_pump_speed_2": NumberSpec(49, 10, 100, 5, 1.0, "%"),
    "max_pump_speed_1": NumberSpec(50, 10, 100, 5, 1.0, "%"),
    "max_pump_speed_2": NumberSpec(51, 10, 100, 5, 1.0, "%"),
    "system_cooling_delta_on": NumberSpec(59, 10, 300, 5, 0.1, "K"),
    "system_cooling_delta_off": NumberSpec(62, 5, 295, 5, 0.1, "K"),
    "holiday_temperature": NumberSpec(66, 200, 800, 10, 0.1, "°C"),
    "parallel_loading_delta": NumberSpec(69, 200, 900, 10, 0.1, "K"),
    "max_flow": NumberSpec(89, 5, 1000, 5, 0.1, "L/min"),
    "antifreeze_concentration": NumberSpec(91, 20, 70, 1, 1.0, "%"),
    "circuit_3_minimum_limit": NumberSpec(193, 0, 900, 5, 0.1, "°C"),
    "circuit_3_minimum_recovery": NumberSpec(196, 0, 900, 5, 0.1, "°C"),
    "circuit_3_maximum_limit": NumberSpec(199, 0, 950, 5, 0.1, "°C"),
    "circuit_3_maximum_recovery": NumberSpec(202, 0, 950, 5, 0.1, "°C"),
    "delta_temperature_1_on": NumberSpec(385, 10, 200, 5, 0.1, "K"),
    "delta_temperature_2_on": NumberSpec(388, 10, 200, 5, 0.1, "K"),
    "delta_temperature_3_on": NumberSpec(391, 10, 200, 5, 0.1, "K"),
    "delta_temperature_1_off": NumberSpec(401, 5, 195, 5, 0.1, "K"),
    "delta_temperature_2_off": NumberSpec(404, 5, 195, 5, 0.1, "K"),
    "delta_temperature_3_off": NumberSpec(407, 5, 195, 5, 0.1, "K"),
    "delta_temperature_1_nominal": NumberSpec(411, 15, 300, 5, 0.1, "K"),
    "delta_temperature_2_nominal": NumberSpec(414, 15, 300, 5, 0.1, "K"),
    "delta_temperature_3_nominal": NumberSpec(417, 15, 300, 5, 0.1, "K"),
    "delta_temperature_1_rise": NumberSpec(421, 10, 200, 10, 0.1, "K"),
    "delta_temperature_2_rise": NumberSpec(424, 10, 200, 10, 0.1, "K"),
    "delta_temperature_3_rise": NumberSpec(427, 10, 200, 10, 0.1, "K"),
    "max_storage_temperature_2": NumberSpec(438, 40, 950, 10, 0.1, "°C"),
    "max_storage_temperature_1": NumberSpec(444, 40, 950, 10, 0.1, "°C"),
    "collector_emergency_temperature_1": NumberSpec(448, 800, 2000, 10, 0.1, "°C"),
    "collector_emergency_temperature_2": NumberSpec(451, 800, 2000, 10, 0.1, "°C"),
    "max_collector_temperature_1": NumberSpec(455, 700, 1600, 10, 0.1, "°C"),
    "max_collector_temperature_2": NumberSpec(458, 700, 1600, 10, 0.1, "°C"),
    "min_collector_temperature_1": NumberSpec(497, 100, 900, 5, 0.1, "°C"),
    "min_collector_temperature_2": NumberSpec(500, 100, 900, 5, 0.1, "°C"),
    "collector_frost_temperature_1": NumberSpec(529, -400, 100, 5, 0.1, "°C"),
    "collector_frost_temperature_2": NumberSpec(532, -400, 100, 5, 0.1, "°C"),
    "disinfection_period": NumberSpec(2561, 0, 720, 1, 1.0, "h"),
    "disinfection_temperature": NumberSpec(2563, 0, 950, 10, 0.1, "°C"),
    "disinfection_duration": NumberSpec(2565, 0, 1439, 1, 1.0, "min"),
    "disinfection_start_time": NumberSpec(2566, 0, 1440, 1, 1.0, "min"),
    "drainback_activation_time": NumberSpec(2769, 1, 100, 1, 1.0, "s"),
    "drainback_fill_time": NumberSpec(2771, 10, 300, 5, 0.1, "min"),
    "drainback_stabilisation_time": NumberSpec(2773, 10, 150, 5, 0.1, "min"),
    "thermostat_on_temperature": NumberSpec(3585, 0, 950, 5, 0.1, "°C"),
    "thermostat_start_time_1": NumberSpec(3587, 0, 1439, 15, 1.0, "min"),
    "thermostat_off_temperature": NumberSpec(3589, 0, 950, 5, 0.1, "°C"),
    "thermostat_end_time_1": NumberSpec(3592, 0, 1439, 15, 1.0, "min"),
    "thermostat_start_time_2": NumberSpec(3593, 0, 1439, 15, 1.0, "min"),
    "thermostat_end_time_2": NumberSpec(3594, 0, 1439, 15, 1.0, "min"),
    "thermostat_start_time_3": NumberSpec(3595, 0, 1439, 15, 1.0, "min"),
    "thermostat_end_time_3": NumberSpec(3596, 0, 1439, 15, 1.0, "min"),
    "tube_collector_start_time": NumberSpec(3904, 0, 1439, 15, 1.0, "min"),
    "tube_collector_end_time": NumberSpec(3905, 0, 1439, 15, 1.0, "min"),
    "tube_collector_run_time": NumberSpec(3906, 5, 500, 5, 1.0, "s"),
    "tube_collector_interval": NumberSpec(3907, 1, 60, 1, 1.0, "min"),
}
