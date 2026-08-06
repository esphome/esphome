from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_HIGH,
    CONF_ID,
    CONF_LOW,
    CONF_MEDIUM,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_OFFSET_X,
    CONF_OFFSET_Y,
    CONF_OFFSET_Z,
    CONF_RANGE,
    CONF_RESOLUTION,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
)

CODEOWNERS = ["@latonita"]

CONF_LIS2DH12_ID = "lis2dh12_id"

CONF_OUTPUT_DATA_RATE = "output_data_rate"
CONF_MIRROR_Z = "mirror_z"
CONF_ON_ACTIVE = "on_active"
CONF_ON_DOUBLE_TAP = "on_double_tap"
CONF_ON_FREEFALL = "on_freefall"
CONF_ON_ORIENTATION = "on_orientation"
CONF_ON_TAP = "on_tap"

lis2dh12_base_ns = cg.esphome_ns.namespace("lis2dh12_base")
LIS2DH12Component = lis2dh12_base_ns.class_("LIS2DH12Component", cg.PollingComponent)

LIS2DH12Range = lis2dh12_base_ns.enum("Range", True)
RANGES = {
    "2G": LIS2DH12Range.RANGE_2G,
    "4G": LIS2DH12Range.RANGE_4G,
    "8G": LIS2DH12Range.RANGE_8G,
    "16G": LIS2DH12Range.RANGE_16G,
}

LIS2DH12Resolution = lis2dh12_base_ns.enum("Resolution", True)
RESOLUTIONS = {
    CONF_HIGH: LIS2DH12Resolution.HIGH_RESOLUTION,
    CONF_MEDIUM: LIS2DH12Resolution.NORMAL,
    CONF_LOW: LIS2DH12Resolution.LOW_POWER,
}

LIS2DH12DataRate = lis2dh12_base_ns.enum("DataRate", True)
DATA_RATES = {
    "1Hz": LIS2DH12DataRate.RATE_1HZ,
    "10Hz": LIS2DH12DataRate.RATE_10HZ,
    "25Hz": LIS2DH12DataRate.RATE_25HZ,
    "50Hz": LIS2DH12DataRate.RATE_50HZ,
    "100Hz": LIS2DH12DataRate.RATE_100HZ,
    "200Hz": LIS2DH12DataRate.RATE_200HZ,
    "400Hz": LIS2DH12DataRate.RATE_400HZ,
    "1620Hz": LIS2DH12DataRate.RATE_1620HZ_LP,
    "5376Hz": LIS2DH12DataRate.RATE_5376HZ_LP,
}

CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Optional(CONF_RANGE, default="2G"): cv.enum(RANGES, upper=True),
        cv.Optional(CONF_RESOLUTION, default=CONF_HIGH): cv.enum(
            RESOLUTIONS, lower=True
        ),
        cv.Optional(CONF_OUTPUT_DATA_RATE, default="100Hz"): cv.enum(DATA_RATES),
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

LIS2DH12_SENSOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LIS2DH12_ID): cv.use_id(LIS2DH12Component),
    }
)


async def to_code_base(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_range(RANGES[config[CONF_RANGE]]))
    cg.add(var.set_resolution(RESOLUTIONS[config[CONF_RESOLUTION]]))
    cg.add(var.set_output_data_rate(DATA_RATES[config[CONF_OUTPUT_DATA_RATE]]))

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

    for trigger_key, getter in [
        (CONF_ON_TAP, "get_tap_trigger"),
        (CONF_ON_DOUBLE_TAP, "get_double_tap_trigger"),
        (CONF_ON_FREEFALL, "get_freefall_trigger"),
        (CONF_ON_ACTIVE, "get_active_trigger"),
        (CONF_ON_ORIENTATION, "get_orientation_trigger"),
    ]:
        if trigger_key in config:
            await automation.build_automation(
                getattr(var, getter)(),
                [],
                config[trigger_key],
            )

    return var
