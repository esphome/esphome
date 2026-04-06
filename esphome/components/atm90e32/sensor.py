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

from . import atm90e32_ns

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
            device_class=DEVICE_CLASS_POWER,
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


def _validate_threshold_percent(value):
    if isinstance(value, str):
        value = value.strip()
        if value.endswith("%"):
            value = f"{value[:-1].strip()}"
            return cv.float_(value) / 100.0

    value = cv.float_(value)
    if value > 2.0:
        raise cv.Invalid(
            "Percent values above 2.0 must use a '%' suffix, for example 110%."
        )
    return value


def _line_frequency_hz(line_frequency):
    if isinstance(line_frequency, str):
        return float(LINE_FREQS[line_frequency.upper()])
    return float(line_frequency)


THRESHOLDS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ALLOW_UNSAFE_THRESHOLDS, default=False): cv.boolean,
        cv.Optional(CONF_VOLTAGE_NOMINAL): cv.All(
            cv.voltage, cv.Range(min=0.0, min_included=False)
        ),
        cv.Exclusive(CONF_VOLTAGE_SAG_PCT, "voltage_sag_threshold"): cv.All(
            _validate_threshold_percent,
            cv.Range(min=0.0, min_included=False, max=2.0),
        ),
        cv.Exclusive(CONF_VOLTAGE_SAG_V, "voltage_sag_threshold"): cv.All(
            cv.voltage, cv.Range(min=0.0, min_included=False)
        ),
        cv.Exclusive(CONF_VOLTAGE_PEAK_PCT, "voltage_peak_threshold"): cv.All(
            _validate_threshold_percent,
            cv.Range(min=0.0, min_included=False, max=2.0),
        ),
        cv.Exclusive(CONF_VOLTAGE_PEAK_V, "voltage_peak_threshold"): cv.All(
            cv.voltage, cv.Range(min=0.0, min_included=False)
        ),
        cv.Exclusive(CONF_CURRENT_PEAK, "current_peak_threshold"): cv.All(
            cv.current, cv.Range(min=0.0, min_included=False)
        ),
        cv.Optional(CONF_FREQUENCY_LOW_HZ): cv.All(
            cv.frequency, cv.Range(min=0.0, min_included=False)
        ),
        cv.Optional(CONF_FREQUENCY_HIGH_HZ): cv.All(
            cv.frequency, cv.Range(min=0.0, min_included=False)
        ),
    }
)


def _default_voltage_nominal(line_frequency):
    return 120.0 if _line_frequency_hz(line_frequency) == 60.0 else 220.0


def _validate_thresholds(config):
    thresholds = config.get(CONF_THRESHOLDS, {})
    if not thresholds:
        return config

    allow_unsafe = thresholds[CONF_ALLOW_UNSAFE_THRESHOLDS]
    if not allow_unsafe:
        if CONF_VOLTAGE_SAG_PCT in thresholds and not (
            0.50 <= thresholds[CONF_VOLTAGE_SAG_PCT] <= 0.99
        ):
            raise cv.Invalid(
                "thresholds.voltage_sag_pct should usually be between 50% and 99%; "
                "set thresholds.allow_unsafe_thresholds: true to override."
            )
        if CONF_VOLTAGE_PEAK_PCT in thresholds and not (
            1.01 <= thresholds[CONF_VOLTAGE_PEAK_PCT] <= 1.50
        ):
            raise cv.Invalid(
                "thresholds.voltage_peak_pct should usually be between 101% and 150%; "
                "set thresholds.allow_unsafe_thresholds: true to override."
            )

    if (
        CONF_CURRENT_PEAK in thresholds
        and thresholds[CONF_CURRENT_PEAK] > MAX_CURRENT_PEAK_A
    ):
        raise cv.Invalid(
            "thresholds.current_peak must be 65.535A or less because the ATM90E32 "
            "native current register cannot represent a higher threshold. Lowering "
            "gain_ct and scaling published current or power values with filters does "
            "not extend the valid thresholds.current_peak range."
        )

    nominal_voltage = thresholds.get(
        CONF_VOLTAGE_NOMINAL, _default_voltage_nominal(config[CONF_LINE_FREQUENCY])
    )
    sag_voltage = thresholds.get(
        CONF_VOLTAGE_SAG_V,
        nominal_voltage * thresholds.get(CONF_VOLTAGE_SAG_PCT, DEFAULT_VOLTAGE_SAG_PCT),
    )
    peak_voltage = thresholds.get(
        CONF_VOLTAGE_PEAK_V,
        nominal_voltage
        * thresholds.get(CONF_VOLTAGE_PEAK_PCT, DEFAULT_VOLTAGE_PEAK_PCT),
    )

    if not sag_voltage < nominal_voltage < peak_voltage:
        raise cv.Invalid(
            "Invalid voltage thresholds: expected sag < nominal < peak "
            f"(got {sag_voltage:.3f}V < {nominal_voltage:.3f}V < {peak_voltage:.3f}V)."
        )

    nominal_frequency = _line_frequency_hz(config[CONF_LINE_FREQUENCY])
    frequency_low = thresholds.get(
        CONF_FREQUENCY_LOW_HZ, nominal_frequency - DEFAULT_FREQUENCY_THRESHOLD_BAND_HZ
    )
    frequency_high = thresholds.get(
        CONF_FREQUENCY_HIGH_HZ, nominal_frequency + DEFAULT_FREQUENCY_THRESHOLD_BAND_HZ
    )

    if not frequency_low < frequency_high:
        raise cv.Invalid(
            "Invalid frequency thresholds: thresholds.frequency_low_hz must be lower than "
            "thresholds.frequency_high_hz."
        )

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
            cv.Optional(CONF_THRESHOLDS): THRESHOLDS_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema()),
    _validate_thresholds,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
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
    if frequency_config := config.get(CONF_FREQUENCY):
        sens = await sensor.new_sensor(frequency_config)
        cg.add(var.set_freq_sensor(sens))
    if chip_temperature_config := config.get(CONF_CHIP_TEMPERATURE):
        sens = await sensor.new_sensor(chip_temperature_config)
        cg.add(var.set_chip_temperature_sensor(sens))
    cg.add(var.set_line_freq(config[CONF_LINE_FREQUENCY]))
    cg.add(var.set_current_phases(config[CONF_CURRENT_PHASES]))
    cg.add(var.set_pga_gain(config[CONF_GAIN_PGA]))
    cg.add(var.set_peak_current_signed(config[CONF_PEAK_CURRENT_SIGNED]))
    cg.add(var.set_enable_offset_calibration(config[CONF_ENABLE_OFFSET_CALIBRATION]))
    cg.add(var.set_enable_gain_calibration(config[CONF_ENABLE_GAIN_CALIBRATION]))

    if thresholds := config.get(CONF_THRESHOLDS):
        if CONF_VOLTAGE_NOMINAL in thresholds:
            cg.add(var.set_threshold_voltage_nominal(thresholds[CONF_VOLTAGE_NOMINAL]))
        if CONF_VOLTAGE_SAG_PCT in thresholds:
            cg.add(var.set_threshold_voltage_sag_pct(thresholds[CONF_VOLTAGE_SAG_PCT]))
        if CONF_VOLTAGE_SAG_V in thresholds:
            cg.add(var.set_threshold_voltage_sag_v(thresholds[CONF_VOLTAGE_SAG_V]))
        if CONF_VOLTAGE_PEAK_PCT in thresholds:
            cg.add(
                var.set_threshold_voltage_peak_pct(thresholds[CONF_VOLTAGE_PEAK_PCT])
            )
        if CONF_VOLTAGE_PEAK_V in thresholds:
            cg.add(var.set_threshold_voltage_peak_v(thresholds[CONF_VOLTAGE_PEAK_V]))
        if CONF_CURRENT_PEAK in thresholds:
            cg.add(var.set_threshold_current_peak_a(thresholds[CONF_CURRENT_PEAK]))
        if CONF_FREQUENCY_LOW_HZ in thresholds:
            cg.add(
                var.set_threshold_frequency_low_hz(thresholds[CONF_FREQUENCY_LOW_HZ])
            )
        if CONF_FREQUENCY_HIGH_HZ in thresholds:
            cg.add(
                var.set_threshold_frequency_high_hz(thresholds[CONF_FREQUENCY_HIGH_HZ])
            )
