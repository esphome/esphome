import logging
import math

import esphome.codegen as cg
from esphome.components import sensor, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_APPARENT_POWER,
    CONF_CURRENT,
    CONF_FORWARD_ACTIVE_ENERGY,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_LINE_FREQUENCY,
    CONF_PHASE_A,
    CONF_PHASE_ANGLE,
    CONF_PHASE_B,
    CONF_PHASE_C,
    CONF_POWER,
    CONF_POWER_FACTOR,
    CONF_REACTIVE_POWER,
    CONF_REVERSE_ACTIVE_ENERGY,
    CONF_VOLTAGE,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_CURRENT_AC,
    ICON_LIGHTBULB,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_DEGREES,
    UNIT_HERTZ,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)
from esphome.types import ConfigType

from . import atm90e32_ns

_LOGGER = logging.getLogger(__name__)

CONF_CHIP_TEMPERATURE = "chip_temperature"
CONF_GAIN_PGA = "gain_pga"
CONF_CURRENT_PHASES = "current_phases"
CONF_GAIN_VOLTAGE = "gain_voltage"
CONF_GAIN_CT = "gain_ct"
CONF_OFFSET_VOLTAGE = "offset_voltage"
CONF_OFFSET_CURRENT = "offset_current"
CONF_OFFSET_ACTIVE_POWER = "offset_active_power"
CONF_OFFSET_REACTIVE_POWER = "offset_reactive_power"
CONF_HARMONIC_POWER = "harmonic_power"
CONF_PEAK_CURRENT = "peak_current"
CONF_PEAK_CURRENT_SIGNED = "peak_current_signed"
CONF_ENABLE_OFFSET_CALIBRATION = "enable_offset_calibration"
CONF_ENABLE_GAIN_CALIBRATION = "enable_gain_calibration"
CONF_PHASE_STATUS = "phase_status"
CONF_FREQUENCY_STATUS = "frequency_status"
CONF_THRESHOLDS = "thresholds"
CONF_ALLOW_UNSAFE_THRESHOLDS = "allow_unsafe_thresholds"
CONF_VOLTAGE_NOMINAL = "voltage_nominal"
CONF_VOLTAGE_SAG_PCT = "voltage_sag_pct"
CONF_VOLTAGE_PEAK_PCT = "voltage_peak_pct"
CONF_VOLTAGE_SAG_V = "voltage_sag_v"
CONF_VOLTAGE_PEAK_V = "voltage_peak_v"
CONF_CURRENT_PEAK = "current_peak"
CONF_FREQUENCY_LOW_HZ = "frequency_low_hz"
CONF_FREQUENCY_HIGH_HZ = "frequency_high_hz"
UNIT_DEG = "degrees"
LINE_FREQS = {
    "50HZ": 50,
    "60HZ": 60,
}
CURRENT_PHASES = {
    "2": 2,
    "3": 3,
}
PGA_GAINS = {
    "1X": 0x0,
    "2X": 0x15,
    "4X": 0x2A,
}

DEFAULT_VOLTAGE_SAG_PCT = 0.78
DEFAULT_VOLTAGE_PEAK_PCT = 1.22
MAX_CURRENT_PEAK_A = 65.535
MAX_FREQUENCY_HZ = 655.35
DEFAULT_FREQUENCY_THRESHOLD_BAND_HZ = 3.0

ATM90E32Component = atm90e32_ns.class_(
    "ATM90E32Component", cg.PollingComponent, spi.SPIDevice
)

ATM90E32_PHASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_REACTIVE_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
            icon=ICON_LIGHTBULB,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_APPARENT_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT_AMPS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER_FACTOR): sensor.sensor_schema(
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_POWER_FACTOR,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_FORWARD_ACTIVE_ENERGY): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT_HOURS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_REVERSE_ACTIVE_ENERGY): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT_HOURS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_PHASE_ANGLE): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HARMONIC_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PEAK_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_GAIN_VOLTAGE, default=7305): cv.uint16_t,
        cv.Optional(CONF_GAIN_CT, default=27961): cv.uint16_t,
        cv.Optional(CONF_OFFSET_VOLTAGE, default=0): cv.int_,
        cv.Optional(CONF_OFFSET_CURRENT, default=0): cv.int_,
        cv.Optional(CONF_OFFSET_ACTIVE_POWER, default=0): cv.int_,
        cv.Optional(CONF_OFFSET_REACTIVE_POWER, default=0): cv.int_,
    }
)


POSITIVE_VOLTAGE = cv.All(cv.voltage, cv.positive_not_null_float)
THRESHOLD_PERCENT = cv.All(
    cv.unbounded_percentage, cv.float_range(min=0, min_included=False, max=2.0)
)
REGISTER_FREQUENCY = cv.All(
    cv.frequency, cv.float_range(min=0, min_included=False, max=MAX_FREQUENCY_HZ)
)

THRESHOLDS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ALLOW_UNSAFE_THRESHOLDS, default=False): cv.boolean,
        cv.Optional(CONF_VOLTAGE_NOMINAL): POSITIVE_VOLTAGE,
        cv.Exclusive(CONF_VOLTAGE_SAG_PCT, "voltage_sag_threshold"): THRESHOLD_PERCENT,
        cv.Exclusive(CONF_VOLTAGE_SAG_V, "voltage_sag_threshold"): POSITIVE_VOLTAGE,
        cv.Exclusive(
            CONF_VOLTAGE_PEAK_PCT, "voltage_peak_threshold"
        ): THRESHOLD_PERCENT,
        cv.Exclusive(CONF_VOLTAGE_PEAK_V, "voltage_peak_threshold"): POSITIVE_VOLTAGE,
        cv.Optional(CONF_CURRENT_PEAK): cv.All(
            cv.current,
            cv.float_range(min=0, min_included=False, max=MAX_CURRENT_PEAK_A),
        ),
        cv.Optional(CONF_FREQUENCY_LOW_HZ): REGISTER_FREQUENCY,
        cv.Optional(CONF_FREQUENCY_HIGH_HZ): REGISTER_FREQUENCY,
    }
)


def _default_voltage_nominal(line_frequency: int) -> float:
    return 120.0 if line_frequency == 60 else 220.0


def _resolve_thresholds(config: ConfigType) -> ConfigType:
    line_frequency = config[CONF_LINE_FREQUENCY].enum_value
    resolved = dict(config[CONF_THRESHOLDS])
    nominal_voltage = resolved.setdefault(
        CONF_VOLTAGE_NOMINAL, _default_voltage_nominal(line_frequency)
    )
    for volts_key, pct_key, default_pct in (
        (CONF_VOLTAGE_SAG_V, CONF_VOLTAGE_SAG_PCT, DEFAULT_VOLTAGE_SAG_PCT),
        (CONF_VOLTAGE_PEAK_V, CONF_VOLTAGE_PEAK_PCT, DEFAULT_VOLTAGE_PEAK_PCT),
    ):
        resolved.setdefault(
            volts_key, nominal_voltage * resolved.get(pct_key, default_pct)
        )
    resolved.setdefault(
        CONF_FREQUENCY_LOW_HZ, line_frequency - DEFAULT_FREQUENCY_THRESHOLD_BAND_HZ
    )
    resolved.setdefault(
        CONF_FREQUENCY_HIGH_HZ, line_frequency + DEFAULT_FREQUENCY_THRESHOLD_BAND_HZ
    )
    return resolved


def _validate_thresholds(config: ConfigType) -> ConfigType:
    configured = config[CONF_THRESHOLDS]
    thresholds = _resolve_thresholds(config)
    nominal_voltage = thresholds[CONF_VOLTAGE_NOMINAL]
    sag_voltage = thresholds[CONF_VOLTAGE_SAG_V]
    peak_voltage = thresholds[CONF_VOLTAGE_PEAK_V]

    if not thresholds[CONF_ALLOW_UNSAFE_THRESHOLDS]:
        for key, ratio, low, high in (
            (CONF_VOLTAGE_SAG_V, sag_voltage / nominal_voltage, 0.50, 0.99),
            (CONF_VOLTAGE_PEAK_V, peak_voltage / nominal_voltage, 1.01, 1.50),
        ):
            if not low <= ratio <= high:
                raise cv.Invalid(
                    f"thresholds.{key} must be between {low:.0%} and {high:.0%} of voltage_nominal; "
                    "set thresholds.allow_unsafe_thresholds: true to override."
                )

    if not sag_voltage < nominal_voltage < peak_voltage:
        raise cv.Invalid(
            "Invalid voltage thresholds: expected sag < nominal < peak "
            f"(got {sag_voltage:.3f}V < {nominal_voltage:.3f}V < {peak_voltage:.3f}V)."
        )

    frequency_low = thresholds[CONF_FREQUENCY_LOW_HZ]
    frequency_high = thresholds[CONF_FREQUENCY_HIGH_HZ]

    if round(frequency_low * 100) >= round(frequency_high * 100):
        raise cv.Invalid(
            "thresholds.frequency_low_hz must be lower than thresholds.frequency_high_hz "
            "at the register resolution of 0.01Hz."
        )

    for gain_key, triggers, registers, scale in (
        (
            CONF_GAIN_VOLTAGE,
            (
                CONF_VOLTAGE_NOMINAL,
                CONF_VOLTAGE_SAG_PCT,
                CONF_VOLTAGE_PEAK_PCT,
                CONF_VOLTAGE_SAG_V,
                CONF_VOLTAGE_PEAK_V,
            ),
            ((CONF_VOLTAGE_SAG_V, min), (CONF_VOLTAGE_PEAK_V, max)),
            100,
        ),
        (CONF_GAIN_CT, (CONF_CURRENT_PEAK,), ((CONF_CURRENT_PEAK, max),), 1000),
    ):
        user_set = any(key in configured for key in triggers)
        # A legacy config without current_peak keeps the fixed OITH register value.
        if not user_set and gain_key == CONF_GAIN_CT:
            continue
        gains = {
            config[phase][gain_key]
            for phase in (CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C)
            if phase in config
        }
        if not gains or 0 in gains:
            if not user_set:
                continue
            raise cv.Invalid(
                f"Thresholds require at least one phase and a nonzero {gain_key} on every configured phase."
            )
        for key, select_gain in registers:
            gain = select_gain(gains)
            limit = 65535 * gain / (scale * math.sqrt(2) * 16384)
            if thresholds[key] <= limit:
                continue
            message = (
                f"thresholds.{key} ({thresholds[key]:.1f}) cannot fit the 16-bit threshold register with "
                f"{gain_key}: {gain}; the highest value that fits is {limit:.1f}"
            )
            if user_set:
                raise cv.Invalid(
                    f"{message}. Increase {gain_key} or lower the threshold."
                )
            # Defaults are only warned about so existing configs keep building; the register is clamped.
            _LOGGER.warning(
                "%s. The default is clamped to the register maximum, so this alarm cannot trigger; "
                "set thresholds.%s to at most %.1f to use it.",
                message,
                key,
                limit,
            )

    config[CONF_THRESHOLDS] = thresholds
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ATM90E32Component),
            cv.Optional(CONF_PHASE_A): ATM90E32_PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_B): ATM90E32_PHASE_SCHEMA,
            cv.Optional(CONF_PHASE_C): ATM90E32_PHASE_SCHEMA,
            cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                icon=ICON_CURRENT_AC,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_FREQUENCY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CHIP_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Required(CONF_LINE_FREQUENCY): cv.enum(LINE_FREQS, upper=True),
            cv.Optional(CONF_CURRENT_PHASES, default="3"): cv.enum(
                CURRENT_PHASES, upper=True
            ),
            cv.Optional(CONF_GAIN_PGA, default="1X"): cv.enum(PGA_GAINS, upper=True),
            cv.Optional(CONF_PEAK_CURRENT_SIGNED, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_OFFSET_CALIBRATION, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_GAIN_CALIBRATION, default=False): cv.boolean,
            cv.Optional(CONF_THRESHOLDS, default={}): THRESHOLDS_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema()),
    _validate_thresholds,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_instance_id(str(config[CONF_ID])))
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    for i, phase in enumerate([CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C]):
        if phase not in config:
            continue
        conf = config[phase]
        cg.add(var.set_volt_gain(i, conf[CONF_GAIN_VOLTAGE]))
        cg.add(var.set_ct_gain(i, conf[CONF_GAIN_CT]))
        cg.add(var.set_voltage_offset(i, conf[CONF_OFFSET_VOLTAGE]))
        cg.add(var.set_current_offset(i, conf[CONF_OFFSET_CURRENT]))
        cg.add(var.set_active_power_offset(i, conf[CONF_OFFSET_ACTIVE_POWER]))
        cg.add(var.set_reactive_power_offset(i, conf[CONF_OFFSET_REACTIVE_POWER]))
        if voltage_config := conf.get(CONF_VOLTAGE):
            sens = await sensor.new_sensor(voltage_config)
            cg.add(var.set_voltage_sensor(i, sens))
        if current_config := conf.get(CONF_CURRENT):
            sens = await sensor.new_sensor(current_config)
            cg.add(var.set_current_sensor(i, sens))
        if power_config := conf.get(CONF_POWER):
            sens = await sensor.new_sensor(power_config)
            cg.add(var.set_power_sensor(i, sens))
        if reactive_power_config := conf.get(CONF_REACTIVE_POWER):
            sens = await sensor.new_sensor(reactive_power_config)
            cg.add(var.set_reactive_power_sensor(i, sens))
        if apparent_power_config := conf.get(CONF_APPARENT_POWER):
            sens = await sensor.new_sensor(apparent_power_config)
            cg.add(var.set_apparent_power_sensor(i, sens))
        if power_factor_config := conf.get(CONF_POWER_FACTOR):
            sens = await sensor.new_sensor(power_factor_config)
            cg.add(var.set_power_factor_sensor(i, sens))
        if forward_active_energy_config := conf.get(CONF_FORWARD_ACTIVE_ENERGY):
            sens = await sensor.new_sensor(forward_active_energy_config)
            cg.add(var.set_forward_active_energy_sensor(i, sens))
        if reverse_active_energy_config := conf.get(CONF_REVERSE_ACTIVE_ENERGY):
            sens = await sensor.new_sensor(reverse_active_energy_config)
            cg.add(var.set_reverse_active_energy_sensor(i, sens))
        if phase_angle_config := conf.get(CONF_PHASE_ANGLE):
            sens = await sensor.new_sensor(phase_angle_config)
            cg.add(var.set_phase_angle_sensor(i, sens))
        if harmonic_active_power_config := conf.get(CONF_HARMONIC_POWER):
            sens = await sensor.new_sensor(harmonic_active_power_config)
            cg.add(var.set_harmonic_active_power_sensor(i, sens))
        if peak_current_config := conf.get(CONF_PEAK_CURRENT):
            sens = await sensor.new_sensor(peak_current_config)
            cg.add(var.set_peak_current_sensor(i, sens))
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_FREQUENCY, var.set_freq_sensor)
    await sensors(CONF_CHIP_TEMPERATURE, var.set_chip_temperature_sensor)
    cg.add(var.set_line_freq(config[CONF_LINE_FREQUENCY]))
    cg.add(var.set_current_phases(config[CONF_CURRENT_PHASES]))
    cg.add(var.set_pga_gain(config[CONF_GAIN_PGA]))
    cg.add(var.set_peak_current_signed(config[CONF_PEAK_CURRENT_SIGNED]))
    cg.add(var.set_enable_offset_calibration(config[CONF_ENABLE_OFFSET_CALIBRATION]))
    cg.add(var.set_enable_gain_calibration(config[CONF_ENABLE_GAIN_CALIBRATION]))

    thresholds = config[CONF_THRESHOLDS]
    cg.add(var.set_threshold_voltage_sag_v(thresholds[CONF_VOLTAGE_SAG_V]))
    cg.add(var.set_threshold_voltage_peak_v(thresholds[CONF_VOLTAGE_PEAK_V]))
    cg.add_optional(var.set_threshold_current_peak_a, thresholds.get(CONF_CURRENT_PEAK))
    cg.add(
        var.set_frequency_thresholds(
            round(thresholds[CONF_FREQUENCY_LOW_HZ] * 100),
            round(thresholds[CONF_FREQUENCY_HIGH_HZ] * 100),
        )
    )
