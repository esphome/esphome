from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_MODE, CONF_RANGE, CONF_TRIGGER_ID

CODEOWNERS = ["@jthoward64"]

CONF_LIS3DH_ID = "lis3dh_id"
CONF_ODR = "odr"
CONF_FIFO_ENABLED = "fifo_enabled"
CONF_FIFO_MODE = "fifo_mode"
CONF_FIFO_WATERMARK = "fifo_watermark"
CONF_INT1_PIN = "int1_pin"
CONF_INT2_PIN = "int2_pin"
CONF_TAP_ENABLED = "tap_enabled"
CONF_TAP_THRESHOLD = "tap_threshold"
CONF_TAP_SHOCK_DURATION = "tap_shock_duration"
CONF_TAP_QUIET_DURATION = "tap_quiet_duration"
CONF_TAP_DOUBLE_TAP_TIMEOUT = "tap_double_tap_timeout"
CONF_ACTIVITY_ENABLED = "activity_enabled"
CONF_ACTIVITY_THRESHOLD = "activity_threshold"
CONF_ACTIVITY_DURATION = "activity_duration"
CONF_ACTIVITY_INTERRUPT_MODE = "activity_interrupt_mode"
CONF_FREEFALL_ENABLED = "freefall_enabled"
CONF_FREEFALL_THRESHOLD = "freefall_threshold"
CONF_FREEFALL_DURATION = "freefall_duration"
CONF_FREEFALL_INTERRUPT_MODE = "freefall_interrupt_mode"
CONF_ENABLE_DEEP_SLEEP_WAKEUP = "enable_deep_sleep_wakeup"
CONF_AUTO_LOW_POWER_ENABLED = "auto_low_power_enabled"
CONF_AUTO_LOW_POWER_THRESHOLD = "auto_low_power_threshold"
CONF_AUTO_LOW_POWER_DURATION = "auto_low_power_duration"
CONF_HIGH_PASS_FILTER_ENABLED = "high_pass_filter_enabled"
CONF_HIGH_PASS_FILTER_MODE = "high_pass_filter_mode"
CONF_HIGH_PASS_FILTER_CUTOFF = "high_pass_filter_cutoff"
CONF_TEMPERATURE_ENABLED = "temperature_enabled"
CONF_ON_TAP = "on_tap"
CONF_ON_DOUBLE_TAP = "on_double_tap"
CONF_ON_ACTIVITY = "on_activity"
CONF_ON_FREEFALL = "on_freefall"
CONF_ON_ORIENTATION_CHANGE = "on_orientation_change"

lis3dh_ns = cg.esphome_ns.namespace("lis3dh")
LIS3DHComponent = lis3dh_ns.class_("LIS3DHComponent", cg.PollingComponent)

LIS3DHRange = lis3dh_ns.enum("LIS3DHRange", is_class=True)
LIS3DHMode = lis3dh_ns.enum("LIS3DHMode", is_class=True)
LIS3DHDataRate = lis3dh_ns.enum("LIS3DHDataRate", is_class=True)
LIS3DHFifoMode = lis3dh_ns.enum("LIS3DHFifoMode", is_class=True)
LIS3DHInterruptMode = lis3dh_ns.enum("LIS3DHInterruptMode", is_class=True)

TapTrigger = lis3dh_ns.class_("TapTrigger", automation.Trigger.template())
DoubleTapTrigger = lis3dh_ns.class_("DoubleTapTrigger", automation.Trigger.template())
ActivityTrigger = lis3dh_ns.class_("ActivityTrigger", automation.Trigger.template())
FreefallTrigger = lis3dh_ns.class_("FreefallTrigger", automation.Trigger.template())
OrientationChangeTrigger = lis3dh_ns.class_(
    "OrientationChangeTrigger", automation.Trigger.template()
)

RANGE_OPTIONS = {
    "2G": LIS3DHRange.RANGE_2G,
    "4G": LIS3DHRange.RANGE_4G,
    "8G": LIS3DHRange.RANGE_8G,
    "16G": LIS3DHRange.RANGE_16G,
}

MODE_OPTIONS = {
    "low_power": LIS3DHMode.MODE_LOW_POWER,
    "normal": LIS3DHMode.MODE_NORMAL,
    "high_resolution": LIS3DHMode.MODE_HIGH_RESOLUTION,
}

DATA_RATE_OPTIONS = {
    "1Hz": LIS3DHDataRate.ODR_1HZ,
    "10Hz": LIS3DHDataRate.ODR_10HZ,
    "25Hz": LIS3DHDataRate.ODR_25HZ,
    "50Hz": LIS3DHDataRate.ODR_50HZ,
    "100Hz": LIS3DHDataRate.ODR_100HZ,
    "200Hz": LIS3DHDataRate.ODR_200HZ,
    "400Hz": LIS3DHDataRate.ODR_400HZ,
    "1600Hz": LIS3DHDataRate.ODR_1600HZ,
    "5376Hz": LIS3DHDataRate.ODR_5376HZ,
}

FIFO_MODE_OPTIONS = {
    "bypass": LIS3DHFifoMode.FIFO_BYPASS,
    "fifo": LIS3DHFifoMode.FIFO_MODE,
    "stream": LIS3DHFifoMode.FIFO_STREAM,
    "stream_to_fifo": LIS3DHFifoMode.FIFO_STREAM_TO_FIFO,
}

# See LIS3DH datasheet Table 64 "Interrupt mode".
INTERRUPT_MODE_OPTIONS = {
    "or": LIS3DHInterruptMode.OR,
    "movement_6d": LIS3DHInterruptMode.MOVEMENT_6D,
    "and": LIS3DHInterruptMode.AND,
    "position_6d": LIS3DHInterruptMode.POSITION_6D,
}


CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Optional(CONF_RANGE, default="2G"): cv.enum(RANGE_OPTIONS, upper=True),
        cv.Optional(CONF_MODE, default="normal"): cv.enum(MODE_OPTIONS, lower=True),
        cv.Optional(CONF_ODR, default="100Hz"): cv.enum(DATA_RATE_OPTIONS),
        cv.Optional(CONF_FIFO_ENABLED, default=True): cv.boolean,
        # "stream_to_fifo" runs in stream mode until a trigger event, then switches into
        # "fifo" mode and permanently stops collecting new samples once full -- a one-shot
        # "freeze a snapshot around the trigger" mode. Since the LIS3DH's OUT_X/Y/Z registers
        # are the FIFO's read window, that stop makes acceleration readings appear frozen
        # forever the moment any trigger (e.g. activity detection) fires. "stream" is a plain
        # circular buffer that never stops, which is what continuous live sensing needs.
        cv.Optional(CONF_FIFO_MODE, default="stream"): cv.enum(
            FIFO_MODE_OPTIONS, lower=True
        ),
        cv.Optional(CONF_FIFO_WATERMARK, default=25): cv.int_range(1, 32),
        cv.Optional(CONF_INT1_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_INT2_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_TAP_ENABLED, default=False): cv.boolean,
        cv.Optional(CONF_TAP_THRESHOLD, default=0x12): cv.int_range(0, 255),
        cv.Optional(CONF_TAP_SHOCK_DURATION, default=0x20): cv.int_range(0, 255),
        cv.Optional(CONF_TAP_QUIET_DURATION, default=0x20): cv.int_range(0, 255),
        cv.Optional(CONF_TAP_DOUBLE_TAP_TIMEOUT, default=0x30): cv.int_range(0, 255),
        cv.Optional(CONF_ACTIVITY_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_ACTIVITY_THRESHOLD, default=0x10): cv.int_range(0, 255),
        cv.Optional(CONF_ACTIVITY_DURATION, default=0x05): cv.int_range(0, 255),
        cv.Optional(CONF_ACTIVITY_INTERRUPT_MODE, default="or"): cv.enum(
            INTERRUPT_MODE_OPTIONS, lower=True
        ),
        cv.Optional(CONF_FREEFALL_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_FREEFALL_THRESHOLD, default=0x08): cv.int_range(0, 255),
        cv.Optional(CONF_FREEFALL_DURATION, default=0x01): cv.int_range(0, 255),
        cv.Optional(CONF_FREEFALL_INTERRUPT_MODE, default="and"): cv.enum(
            INTERRUPT_MODE_OPTIONS, lower=True
        ),
        cv.Optional(CONF_ENABLE_DEEP_SLEEP_WAKEUP, default=False): cv.boolean,
        cv.Optional(CONF_AUTO_LOW_POWER_ENABLED, default=False): cv.boolean,
        cv.Optional(CONF_AUTO_LOW_POWER_THRESHOLD, default=0x10): cv.int_range(0, 255),
        cv.Optional(CONF_AUTO_LOW_POWER_DURATION, default=0x05): cv.int_range(0, 255),
        cv.Optional(CONF_HIGH_PASS_FILTER_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_HIGH_PASS_FILTER_MODE, default=0): cv.int_range(0, 3),
        cv.Optional(CONF_HIGH_PASS_FILTER_CUTOFF, default=0): cv.int_range(0, 3),
        cv.Optional(CONF_TEMPERATURE_ENABLED, default=False): cv.boolean,
        cv.Optional(CONF_ON_TAP): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TapTrigger)}
        ),
        cv.Optional(CONF_ON_DOUBLE_TAP): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DoubleTapTrigger)}
        ),
        cv.Optional(CONF_ON_ACTIVITY): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ActivityTrigger)}
        ),
        cv.Optional(CONF_ON_FREEFALL): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FreefallTrigger)}
        ),
        cv.Optional(CONF_ON_ORIENTATION_CHANGE): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OrientationChangeTrigger)}
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def setup_lis3dh_base(var, config):
    """Apply all base LIS3DH configuration to a variable created by a platform component."""
    cg.add(var.set_accel_range(config[CONF_RANGE]))
    cg.add(var.set_operating_mode(config[CONF_MODE]))
    cg.add(var.set_odr(config[CONF_ODR]))

    cg.add(var.set_fifo_enabled(config[CONF_FIFO_ENABLED]))
    cg.add(var.set_fifo_mode(config[CONF_FIFO_MODE]))
    cg.add(var.set_fifo_watermark(config[CONF_FIFO_WATERMARK]))

    if CONF_INT1_PIN in config:
        int1_pin = await cg.gpio_pin_expression(config[CONF_INT1_PIN])
        cg.add(var.set_int1_pin(int1_pin))
    if CONF_INT2_PIN in config:
        int2_pin = await cg.gpio_pin_expression(config[CONF_INT2_PIN])
        cg.add(var.set_int2_pin(int2_pin))

    cg.add(var.set_tap_enabled(config[CONF_TAP_ENABLED]))
    cg.add(var.set_tap_threshold(config[CONF_TAP_THRESHOLD]))
    cg.add(var.set_tap_shock_duration(config[CONF_TAP_SHOCK_DURATION]))
    cg.add(var.set_tap_quiet_duration(config[CONF_TAP_QUIET_DURATION]))
    cg.add(var.set_tap_double_tap_timeout(config[CONF_TAP_DOUBLE_TAP_TIMEOUT]))

    cg.add(var.set_activity_enabled(config[CONF_ACTIVITY_ENABLED]))
    cg.add(var.set_activity_threshold(config[CONF_ACTIVITY_THRESHOLD]))
    cg.add(var.set_activity_duration(config[CONF_ACTIVITY_DURATION]))
    cg.add(var.set_activity_interrupt_mode(config[CONF_ACTIVITY_INTERRUPT_MODE]))

    cg.add(var.set_freefall_enabled(config[CONF_FREEFALL_ENABLED]))
    cg.add(var.set_freefall_threshold(config[CONF_FREEFALL_THRESHOLD]))
    cg.add(var.set_freefall_duration(config[CONF_FREEFALL_DURATION]))
    cg.add(var.set_freefall_interrupt_mode(config[CONF_FREEFALL_INTERRUPT_MODE]))

    cg.add(var.set_enable_deep_sleep_wakeup(config[CONF_ENABLE_DEEP_SLEEP_WAKEUP]))

    cg.add(var.set_auto_low_power_enabled(config[CONF_AUTO_LOW_POWER_ENABLED]))
    cg.add(var.set_auto_low_power_threshold(config[CONF_AUTO_LOW_POWER_THRESHOLD]))
    cg.add(var.set_auto_low_power_duration(config[CONF_AUTO_LOW_POWER_DURATION]))

    cg.add(var.set_high_pass_filter_enabled(config[CONF_HIGH_PASS_FILTER_ENABLED]))
    cg.add(var.set_high_pass_filter_mode(config[CONF_HIGH_PASS_FILTER_MODE]))
    cg.add(var.set_high_pass_filter_cutoff(config[CONF_HIGH_PASS_FILTER_CUTOFF]))

    cg.add(var.set_temperature_enabled(config[CONF_TEMPERATURE_ENABLED]))

    for conf_key in (
        CONF_ON_TAP,
        CONF_ON_DOUBLE_TAP,
        CONF_ON_ACTIVITY,
        CONF_ON_FREEFALL,
        CONF_ON_ORIENTATION_CHANGE,
    ):
        for conf in config.get(conf_key, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, [], conf)
