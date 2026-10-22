from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_ID,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_OFFSET_X,
    CONF_OFFSET_Y,
    CONF_OFFSET_Z,
    CONF_POWER_MODE,
    CONF_RANGE,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@latonita"]
DOMAIN = "lis2dw12"

CONF_LIS2DW12_ID = "lis2dw12_id"

CONF_MIRROR_Z = "mirror_z"
CONF_ON_ACTIVE = "on_active"
CONF_ON_DOUBLE_TAP = "on_double_tap"
CONF_ON_FREEFALL = "on_freefall"
CONF_ON_ORIENTATION = "on_orientation"
CONF_ON_TAP = "on_tap"

CONF_LOW_NOISE = "low_noise"
CONF_FILTER_BANDWIDTH = "filter_bandwidth"
CONF_OUTPUT_DATA_RATE = "output_data_rate"

lis2dw12_ns = cg.esphome_ns.namespace("lis2dw12")
LIS2DW12Component = lis2dw12_ns.class_("LIS2DW12Component", cg.PollingComponent)

LIS2DW12Range = lis2dw12_ns.enum("Range", True)
RANGES = LIS2DW12Range.options("2G", "4G", "8G", "16G", prefix="RANGE_")

LIS2DW12PowerMode = lis2dw12_ns.enum("PowerMode", True)
POWER_MODES = {
    "high_performance": LIS2DW12PowerMode.HIGH_PERF,
    "low_power_1": LIS2DW12PowerMode.LOW_POWER_1,
    "low_power_2": LIS2DW12PowerMode.LOW_POWER_2,
    "low_power_3": LIS2DW12PowerMode.LOW_POWER_3,
    "low_power_4": LIS2DW12PowerMode.LOW_POWER_4,
}

LIS2DW12DataRate = lis2dw12_ns.enum("DataRate", True)
DATA_RATES = LIS2DW12DataRate.options(
    "1.6Hz",
    "12.5Hz",
    "25Hz",
    "50Hz",
    "100Hz",
    "200Hz",
    "400Hz",
    "800Hz",
    "1600Hz",
    prefix="RATE_",
)

LIS2DW12FilterBandwidth = lis2dw12_ns.enum("FilterBandwidth", True)
FILTER_BANDWIDTHS = LIS2DW12FilterBandwidth.options(
    "odr_div_2", "odr_div_4", "odr_div_10", "odr_div_20", prefix="BW_"
)

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_TAP, "add_on_tap_callback"),
    automation.CallbackAutomation(CONF_ON_DOUBLE_TAP, "add_on_double_tap_callback"),
    automation.CallbackAutomation(CONF_ON_FREEFALL, "add_on_freefall_callback"),
    automation.CallbackAutomation(CONF_ON_ACTIVE, "add_on_active_callback"),
    automation.CallbackAutomation(CONF_ON_ORIENTATION, "add_on_orientation_callback"),
)

_OFFSET = cv.float_range(min=-4.5, max=4.5)

# The ODR code means different rates per power mode: 0001 is 1.6 Hz only in low-power modes
# (12.5 Hz in high performance) and every code above 200 Hz runs at 200 Hz in low-power modes
_LOW_POWER_ONLY_RATES = ("1.6Hz",)
_HIGH_PERF_ONLY_RATES = ("400Hz", "800Hz", "1600Hz")


def validate_data_rate(config: ConfigType) -> ConfigType:
    """Reject rates the chip cannot run in the chosen power mode; apply after extending the base schema."""
    rate = str(config[CONF_OUTPUT_DATA_RATE])
    mode = str(config[CONF_POWER_MODE])
    high_perf = mode == "high_performance"
    if (high_perf and rate in _LOW_POWER_ONLY_RATES) or (
        not high_perf and rate in _HIGH_PERF_ONLY_RATES
    ):
        raise cv.Invalid(
            f"{CONF_OUTPUT_DATA_RATE} {rate} is not available with {CONF_POWER_MODE} {mode}",
            [CONF_OUTPUT_DATA_RATE],
        )
    return config


CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LIS2DW12Component),
        cv.Optional(CONF_RANGE, default="2G"): cv.enum(RANGES, upper=True),
        cv.Optional(CONF_POWER_MODE, default="high_performance"): cv.enum(
            POWER_MODES, lower=True
        ),
        cv.Optional(CONF_OUTPUT_DATA_RATE, default="100Hz"): cv.enum(DATA_RATES),
        cv.Optional(CONF_FILTER_BANDWIDTH, default="odr_div_2"): cv.enum(
            FILTER_BANDWIDTHS, lower=True
        ),
        cv.Optional(CONF_LOW_NOISE, default=False): cv.boolean,
        cv.Optional(CONF_CALIBRATION): cv.Schema(
            {
                cv.Optional(CONF_OFFSET_X, default=0): _OFFSET,
                cv.Optional(CONF_OFFSET_Y, default=0): _OFFSET,
                cv.Optional(CONF_OFFSET_Z, default=0): _OFFSET,
            }
        ),
        cv.Optional(CONF_TRANSFORM): cv.Schema(
            {
                cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
                cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
                cv.Optional(CONF_MIRROR_Z, default=False): cv.boolean,
                cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
            }
        ),
        cv.Optional(CONF_ON_TAP): automation.validate_automation({}),
        cv.Optional(CONF_ON_DOUBLE_TAP): automation.validate_automation({}),
        cv.Optional(CONF_ON_FREEFALL): automation.validate_automation({}),
        cv.Optional(CONF_ON_ACTIVE): automation.validate_automation({}),
        cv.Optional(CONF_ON_ORIENTATION): automation.validate_automation({}),
    }
).extend(cv.polling_component_schema("10s"))

LIS2DW12_SENSOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LIS2DW12_ID): cv.use_id(LIS2DW12Component),
    }
)


async def to_code_base(config: ConfigType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_range(config[CONF_RANGE]))
    cg.add(var.set_power_mode(config[CONF_POWER_MODE]))
    cg.add(var.set_output_data_rate(config[CONF_OUTPUT_DATA_RATE]))
    cg.add(var.set_filter_bandwidth(config[CONF_FILTER_BANDWIDTH]))
    cg.add(var.set_low_noise(config[CONF_LOW_NOISE]))

    if transform := config.get(CONF_TRANSFORM):
        cg.add(
            var.set_transform(
                transform[CONF_MIRROR_X],
                transform[CONF_MIRROR_Y],
                transform[CONF_MIRROR_Z],
                transform[CONF_SWAP_XY],
            )
        )

    if calibration_config := config.get(CONF_CALIBRATION):
        cg.add(
            var.set_offset(
                calibration_config[CONF_OFFSET_X],
                calibration_config[CONF_OFFSET_Y],
                calibration_config[CONF_OFFSET_Z],
            )
        )

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)

    return var
