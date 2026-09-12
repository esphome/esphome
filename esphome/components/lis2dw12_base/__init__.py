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

CODEOWNERS = ["@latonita"]

CONF_LIS2DW12_ID = "lis2dw12_id"

CONF_MIRROR_Z = "mirror_z"
CONF_LOW_NOISE = "low_noise"
CONF_FILTER_BANDWIDTH = "filter_bandwidth"
CONF_OUTPUT_DATA_RATE = "output_data_rate"
CONF_ON_ACTIVE = "on_active"
CONF_ON_DOUBLE_TAP = "on_double_tap"
CONF_ON_FREEFALL = "on_freefall"
CONF_ON_ORIENTATION = "on_orientation"
CONF_ON_TAP = "on_tap"

lis2dw12_base_ns = cg.esphome_ns.namespace("lis2dw12_base")
LIS2DW12Component = lis2dw12_base_ns.class_("LIS2DW12Component", cg.PollingComponent)

LIS2DW12Range = lis2dw12_base_ns.enum("Range", True)
RANGES = {
    "2G": LIS2DW12Range.RANGE_2G,
    "4G": LIS2DW12Range.RANGE_4G,
    "8G": LIS2DW12Range.RANGE_8G,
    "16G": LIS2DW12Range.RANGE_16G,
}

LIS2DW12PowerMode = lis2dw12_base_ns.enum("PowerMode", True)
POWER_MODES = {
    "high_performance": LIS2DW12PowerMode.HIGH_PERF,
    "low_power_1": LIS2DW12PowerMode.LOW_POWER_1,
    "low_power_2": LIS2DW12PowerMode.LOW_POWER_2,
    "low_power_3": LIS2DW12PowerMode.LOW_POWER_3,
    "low_power_4": LIS2DW12PowerMode.LOW_POWER_4,
}

LIS2DW12DataRate = lis2dw12_base_ns.enum("DataRate", True)
DATA_RATES = {
    "1.6Hz": LIS2DW12DataRate.RATE_1_6HZ,
    "12.5Hz": LIS2DW12DataRate.RATE_12_5HZ,
    "25Hz": LIS2DW12DataRate.RATE_25HZ,
    "50Hz": LIS2DW12DataRate.RATE_50HZ,
    "100Hz": LIS2DW12DataRate.RATE_100HZ,
    "200Hz": LIS2DW12DataRate.RATE_200HZ,
    "400Hz": LIS2DW12DataRate.RATE_400HZ,
    "800Hz": LIS2DW12DataRate.RATE_800HZ,
    "1600Hz": LIS2DW12DataRate.RATE_1600HZ,
}

LIS2DW12FilterBandwidth = lis2dw12_base_ns.enum("FilterBandwidth", True)
FILTER_BANDWIDTHS = {
    "odr_div_2": LIS2DW12FilterBandwidth.BW_ODR_DIV_2,
    "odr_div_4": LIS2DW12FilterBandwidth.BW_ODR_DIV_4,
    "odr_div_10": LIS2DW12FilterBandwidth.BW_ODR_DIV_10,
    "odr_div_20": LIS2DW12FilterBandwidth.BW_ODR_DIV_20,
}

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
                cv.Optional(CONF_OFFSET_X, default=0): cv.float_range(
                    min=-4.5, max=4.5
                ),
                cv.Optional(CONF_OFFSET_Y, default=0): cv.float_range(
                    min=-4.5, max=4.5
                ),
                cv.Optional(CONF_OFFSET_Z, default=0): cv.float_range(
                    min=-4.5, max=4.5
                ),
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
        cv.Optional(CONF_ON_ACTIVE): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_TAP): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_DOUBLE_TAP): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_FREEFALL): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_ORIENTATION): automation.validate_automation(single=True),
    }
).extend(cv.polling_component_schema("10s"))

LIS2DW12_SENSOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LIS2DW12_ID): cv.use_id(LIS2DW12Component),
    }
)


async def to_code_base(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_range(RANGES[config[CONF_RANGE]]))
    cg.add(var.set_power_mode(POWER_MODES[config[CONF_POWER_MODE]]))
    cg.add(var.set_output_data_rate(DATA_RATES[config[CONF_OUTPUT_DATA_RATE]]))
    cg.add(var.set_filter_bandwidth(FILTER_BANDWIDTHS[config[CONF_FILTER_BANDWIDTH]]))
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

    for conf_key, trigger_getter in [
        (CONF_ON_TAP, "get_tap_trigger"),
        (CONF_ON_DOUBLE_TAP, "get_double_tap_trigger"),
        (CONF_ON_FREEFALL, "get_freefall_trigger"),
        (CONF_ON_ACTIVE, "get_active_trigger"),
        (CONF_ON_ORIENTATION, "get_orientation_trigger"),
    ]:
        if conf_key in config:
            await automation.build_automation(
                getattr(var, trigger_getter)(),
                [],
                config[conf_key],
            )

    return var
