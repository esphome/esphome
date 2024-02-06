import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    ICON_BRIEFCASE_DOWNLOAD,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER_PER_SECOND_SQUARED,
)

DEPENDENCIES = ["i2c"]

da217_ns = cg.esphome_ns.namespace("da217")

CONF_ACCEL_X = "accel_x"
CONF_ACCEL_Y = "accel_y"
CONF_ACCEL_Z = "accel_z"

# RESOLUTION_RANGE

CONF_ENABLE_HIGH_PASS_FILTER = "enable_high_pass_filter"
CONF_ENABLE_WATCHDOG = "enable_watchdog"
CONF_WATCHDOG_TIME = "watchdog_time"
CONF_RESOLUTION = "resolution"
CONF_FULL_SCALE = "full_scale"

WatchdogTime = da217_ns.enum("WatchdogTime")
WATCHDOG_TIMES = {
    "1MS": WatchdogTime.Time1ms,
    "50MS": WatchdogTime.Time50ms,
}

Resolution = da217_ns.enum("Resolution")
RESOLUTIONS = {
    "14BITS": Resolution.Resolution14bits,
    "12BITS": Resolution.Resolution12bits,
    "10BITS": Resolution.Resolution10bits,
    "8BITS": Resolution.Resolution8bits,
}

FullScale = da217_ns.enum("WatchdogTime")
FULL_SCALES = {
    "2G": FullScale.PlusMinus2g,
    "4G": FullScale.PlusMinus4g,
    "8G": FullScale.PlusMinus8g,
    "16G": FullScale.PlusMinus16g,
}

# ODR_AXIS

CONF_ENABLE_X_AXIS = "enable_x_axis"
CONF_ENABLE_Y_AXIS = "enable_y_axis"
CONF_ENABLE_Z_AXIS = "enable_z_axis"
CONF_OUTPUT_DATA_RATE = "output_data_rate"

OutputDataRate = da217_ns.enum("OutputDataRate")
OUTPUT_DATA_RATES = {
    "1HZ": OutputDataRate.Rate1Hz,
    "1.95HZ": OutputDataRate.Rate1p95Hz,
    "3.9HZ": OutputDataRate.Rate3p9Hz,
    "7.81HZ": OutputDataRate.Rate7p81Hz,
    "15.63HZ": OutputDataRate.Rate15p63Hz,
    "31.25HZ": OutputDataRate.Rate31p25Hz,
    "62.5HZ": OutputDataRate.Rate62p5Hz,
    "125HZ": OutputDataRate.Rate125Hz,
    "250HZ": OutputDataRate.Rate250Hz,
    "500HZ": OutputDataRate.Rate500Hz,
    "UNCONFIGURED": OutputDataRate.Unconfigured,
}

# INT_SET1

CONF_INTERRUPT_SOURCE = "interrupt_source"
CONF_ENABLE_SINGLE_TAP_INTERRUPT = "enable_single_tap_interrupt"
CONF_ENABLE_DOUBLE_TAP_INTERRUPT = "enable_double_tap_interrupt"
CONF_ENABLE_ORIENTATION_INTERRUPT = "enable_orientation_interrupt"
CONF_ENABLE_ACTIVE_INTERRUPT_Z_AXIS = "enable_active_interrupt_z_axis"
CONF_ENABLE_ACTIVE_INTERRUPT_Y_AXIS = "enable_active_interrupt_y_axis"
CONF_ENABLE_ACTIVE_INTERRUPT_X_AXIS = "enable_active_interrupt_x_axis"

InterruptSource = da217_ns.enum("InterruptSource")
INTERRUPT_SOURCES = {
    "OVERSAMPLING": InterruptSource.Oversampling,
    "UNFILTERED": InterruptSource.Unfiltered,
    "FILTERED": InterruptSource.Filtered,
}

# INT_MAP1

CONF_MAP_SIGNIFICANT_MOVEMENT_INTERRUPT_TO_INT1 = (
    "map_significant_movement_interrupt_to_int1"
)
CONF_MAP_ORIENTATION_INTERRUPT_TO_INT1 = "map_orientation_interrupt_to_int1"
CONF_MAP_SINGLE_TAP_INTERRUPT_TO_INT1 = "map_single_tap_interrupt_to_int1"
CONF_MAP_DOUBLE_TAP_INTERRUPT_TO_INT1 = "map_double_tap_interrupt_to_int1"
CONF_MAP_TILT_INTERRUPT_TO_INT1 = "map_tilt_interrupt_to_int1"
CONF_MAP_ACTIVE_INTERRUPT_TO_INT1 = "map_active_interrupt_to_int1"
CONF_MAP_STEP_COUNTER_INTERRUPT_TO_INT1 = "map_step_counter_interrupt_to_int1"
CONF_MAP_FREEFALL_INTERRUPT_TO_INT1 = "map_freefall_interrupt_to_int1"

# TAP_DUR

CONF_TAP_QUIET_DURATION = "tap_quiet_duration"
CONF_TAP_SHOCK_DURATION = "tap_shock_duration"
CONF_DOUBLE_TAP_DURATION = "double_tap_duration"

TapQuietDuration = da217_ns.enum("TapQuietDuration")
TAP_QUIET_DURATIONS = {
    "30MS": TapQuietDuration.Quiet30ms,
    "20MS": TapQuietDuration.Quiet20ms,
}

TapShockDuration = da217_ns.enum("TapShockDuration")
TAP_SHOCK_DURATIONS = {
    "50MS": TapShockDuration.Shock50ms,
    "70MS": TapShockDuration.Shock70ms,
}

DoubleTapDuration = da217_ns.enum("DoubleTapDuration")
DOUBLE_TAP_DURATIONS = {
    "50MS": DoubleTapDuration.DoubleTap50ms,
    "100MS": DoubleTapDuration.DoubleTap100ms,
    "150MS": DoubleTapDuration.DoubleTap150ms,
    "200MS": DoubleTapDuration.DoubleTap200ms,
    "250MS": DoubleTapDuration.DoubleTap250ms,
    "375MS": DoubleTapDuration.DoubleTap375ms,
    "500MS": DoubleTapDuration.DoubleTap500ms,
    "700MS": DoubleTapDuration.DoubleTap700ms,
}

# TAP_THS

CONF_STABLE_TILT_TIME = "stable_tilt_time"
CONF_TAP_ACCELERATION_THRESHOLD = "tap_acceleration_threshold"

StableTiltTime = da217_ns.enum("StableTiltTime")
STABLE_TILT_TIMES = {
    "32_ODR_PERIODS": StableTiltTime.ODR32,
    "96_ODR_PERIODS": StableTiltTime.ODR96,
    "160_ODR_PERIODS": StableTiltTime.ODR160,
    "224_ODR_PERIODS": StableTiltTime.ODR224,
}

DA217Component = da217_ns.class_("DA217Component", cg.PollingComponent, i2c.I2CDevice)

accel_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_METER_PER_SECOND_SQUARED,
    icon=ICON_BRIEFCASE_DOWNLOAD,
    accuracy_decimals=2,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DA217Component),
            cv.Optional(CONF_ACCEL_X): accel_schema,
            cv.Optional(CONF_ACCEL_Y): accel_schema,
            cv.Optional(CONF_ACCEL_Z): accel_schema,
            # RESOLUTION_RANGE
            cv.Optional(CONF_ENABLE_HIGH_PASS_FILTER, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_WATCHDOG, default=True): cv.boolean,
            cv.Optional(CONF_WATCHDOG_TIME, default="1MS"): cv.enum(
                WATCHDOG_TIMES, upper=True
            ),
            cv.Optional(CONF_RESOLUTION, default="14BITS"): cv.enum(
                RESOLUTIONS, upper=True
            ),
            cv.Optional(CONF_FULL_SCALE, default="2G"): cv.enum(
                FULL_SCALES, upper=True
            ),
            # ODR_AXIS
            cv.Optional(CONF_ENABLE_X_AXIS, default=True): cv.boolean,
            cv.Optional(CONF_ENABLE_Y_AXIS, default=True): cv.boolean,
            cv.Optional(CONF_ENABLE_Z_AXIS, default=True): cv.boolean,
            cv.Optional(CONF_OUTPUT_DATA_RATE, default="UNCONFIGURED"): cv.enum(
                OUTPUT_DATA_RATES, upper=True
            ),
            # INT_SET1
            cv.Optional(CONF_INTERRUPT_SOURCE, default="OVERSAMPLING"): cv.enum(
                INTERRUPT_SOURCES, upper=True
            ),
            cv.Optional(CONF_ENABLE_SINGLE_TAP_INTERRUPT, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_DOUBLE_TAP_INTERRUPT, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_ORIENTATION_INTERRUPT, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_ACTIVE_INTERRUPT_Z_AXIS, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_ACTIVE_INTERRUPT_Y_AXIS, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_ACTIVE_INTERRUPT_X_AXIS, default=False): cv.boolean,
            # INT_MAP1
            cv.Optional(
                CONF_MAP_SIGNIFICANT_MOVEMENT_INTERRUPT_TO_INT1, default=False
            ): cv.boolean,
            cv.Optional(
                CONF_MAP_ORIENTATION_INTERRUPT_TO_INT1, default=False
            ): cv.boolean,
            cv.Optional(
                CONF_MAP_SINGLE_TAP_INTERRUPT_TO_INT1, default=False
            ): cv.boolean,
            cv.Optional(
                CONF_MAP_DOUBLE_TAP_INTERRUPT_TO_INT1, default=False
            ): cv.boolean,
            cv.Optional(CONF_MAP_TILT_INTERRUPT_TO_INT1, default=False): cv.boolean,
            cv.Optional(CONF_MAP_ACTIVE_INTERRUPT_TO_INT1, default=False): cv.boolean,
            cv.Optional(
                CONF_MAP_STEP_COUNTER_INTERRUPT_TO_INT1, default=False
            ): cv.boolean,
            cv.Optional(CONF_MAP_FREEFALL_INTERRUPT_TO_INT1, default=False): cv.boolean,
            # TAP_DUR
            cv.Optional(CONF_TAP_QUIET_DURATION, default="30MS"): cv.enum(
                TAP_QUIET_DURATIONS, upper=True
            ),
            cv.Optional(CONF_TAP_SHOCK_DURATION, default="50MS"): cv.enum(
                TAP_SHOCK_DURATIONS, upper=True
            ),
            cv.Optional(CONF_DOUBLE_TAP_DURATION, default="250MS"): cv.enum(
                DOUBLE_TAP_DURATIONS, upper=True
            ),
            # TAP_THS
            cv.Optional(CONF_STABLE_TILT_TIME, default="32_ODR_PERIODS"): cv.enum(
                STABLE_TILT_TIMES, upper=True
            ),
            cv.Optional(CONF_TAP_ACCELERATION_THRESHOLD, default=0.32): cv.float_range(
                min=0.0, max=1.0
            ),  # Default aims at tap_th=0xA, per datasheet
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x27))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for d in ["x", "y", "z"]:
        accel_key = f"accel_{d}"
        if accel_key in config:
            sens = await sensor.new_sensor(config[accel_key])
            cg.add(getattr(var, f"set_accel_{d}_sensor")(sens))

    cg.add(
        var.set_resolution_range(
            config[CONF_ENABLE_HIGH_PASS_FILTER],
            config[CONF_ENABLE_WATCHDOG],
            config[CONF_WATCHDOG_TIME],
            config[CONF_RESOLUTION],
            config[CONF_FULL_SCALE],
        )
    )
    cg.add(
        var.set_odr_axis(
            not config[CONF_ENABLE_X_AXIS],
            not config[CONF_ENABLE_Y_AXIS],
            not config[CONF_ENABLE_Z_AXIS],
            config[CONF_OUTPUT_DATA_RATE],
        )
    )
    cg.add(
        var.set_int_set1(
            config[CONF_INTERRUPT_SOURCE],
            config[CONF_ENABLE_SINGLE_TAP_INTERRUPT],
            config[CONF_ENABLE_DOUBLE_TAP_INTERRUPT],
            config[CONF_ENABLE_ORIENTATION_INTERRUPT],
            config[CONF_ENABLE_ACTIVE_INTERRUPT_Z_AXIS],
            config[CONF_ENABLE_ACTIVE_INTERRUPT_Y_AXIS],
            config[CONF_ENABLE_ACTIVE_INTERRUPT_X_AXIS],
        )
    )
    cg.add(
        var.set_int_map1(
            config[CONF_MAP_SIGNIFICANT_MOVEMENT_INTERRUPT_TO_INT1],
            config[CONF_MAP_ORIENTATION_INTERRUPT_TO_INT1],
            config[CONF_MAP_SINGLE_TAP_INTERRUPT_TO_INT1],
            config[CONF_MAP_DOUBLE_TAP_INTERRUPT_TO_INT1],
            config[CONF_MAP_TILT_INTERRUPT_TO_INT1],
            config[CONF_MAP_ACTIVE_INTERRUPT_TO_INT1],
            config[CONF_MAP_STEP_COUNTER_INTERRUPT_TO_INT1],
            config[CONF_MAP_FREEFALL_INTERRUPT_TO_INT1],
        )
    )
    cg.add(
        var.set_tap_dur(
            config[CONF_TAP_QUIET_DURATION],
            config[CONF_TAP_SHOCK_DURATION],
            config[CONF_DOUBLE_TAP_DURATION],
        )
    )
    cg.add(
        var.set_tap_ths(
            config[CONF_STABLE_TILT_TIME],
            int(config[CONF_TAP_ACCELERATION_THRESHOLD] * 31 + 0.5),
        )
    )
