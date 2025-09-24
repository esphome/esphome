from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_ID,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_OFFSET_X,
    CONF_OFFSET_Y,
    CONF_OFFSET_Z,
    CONF_RANGE,
    CONF_RESOLUTION,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_TYPE,
)

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["i2c"]

MULTI_CONF = True

CONF_MSA3XX_ID = "msa3xx_id"

CONF_MIRROR_Z = "mirror_z"
CONF_ON_ACTIVE = "on_active"
CONF_ON_DOUBLE_TAP = "on_double_tap"
CONF_ON_FREEFALL = "on_freefall"
CONF_ON_ORIENTATION = "on_orientation"
CONF_ON_TAP = "on_tap"

MODEL_MSA301 = "MSA301"
MODEL_MSA311 = "MSA311"

msa3xx_ns = cg.esphome_ns.namespace("msa3xx")
MSA3xxComponent = msa3xx_ns.class_(
    "MSA3xxComponent", cg.PollingComponent, i2c.I2CDevice
)

MSAModels = msa3xx_ns.enum("Model", True)
MSA_MODELS = {
    MODEL_MSA301: MSAModels.MSA301,
    MODEL_MSA311: MSAModels.MSA311,
}

MSARange = msa3xx_ns.enum("Range", True)
MSA_RANGES = {
    "2G": MSARange.RANGE_2G,
    "4G": MSARange.RANGE_4G,
    "8G": MSARange.RANGE_8G,
    "16G": MSARange.RANGE_16G,
}

MSAResolution = msa3xx_ns.enum("Resolution", True)
RESOLUTIONS_MSA301 = {
    14: MSAResolution.RES_14BIT,
    12: MSAResolution.RES_12BIT,
    10: MSAResolution.RES_10BIT,
    8: MSAResolution.RES_8BIT,
}

RESOLUTIONS_MSA311 = {
    12: MSAResolution.RES_12BIT,
}

_COMMON_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MSA3xxComponent),
        cv.Optional(CONF_RANGE, default="2G"): cv.enum(MSA_RANGES, upper=True),
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


CONFIG_SCHEMA = cv.typed_schema(
    {
        MODEL_MSA301: _COMMON_SCHEMA.extend(
            {
                cv.Optional(CONF_RESOLUTION, default=14): cv.enum(RESOLUTIONS_MSA301),
            }
        ).extend(i2c.i2c_device_schema(0x26)),
        MODEL_MSA311: _COMMON_SCHEMA.extend(
            {
                cv.Optional(CONF_RESOLUTION, default=12): cv.enum(RESOLUTIONS_MSA311),
            }
        ).extend(i2c.i2c_device_schema(0x62)),
    },
    upper=True,
    enum=MSA_MODELS,
)

MSA_SENSOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MSA3XX_ID): cv.use_id(MSA3xxComponent),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_model(config[CONF_TYPE]))
    cg.add(var.set_range(MSA_RANGES[config[CONF_RANGE]]))
    cg.add(var.set_resolution(RESOLUTIONS_MSA301[config[CONF_RESOLUTION]]))

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

    # Triggers secton

    if CONF_ON_ORIENTATION in config:
        await automation.build_automation(
            var.get_orientation_trigger(),
            [],
            config[CONF_ON_ORIENTATION],
        )

    if CONF_ON_TAP in config:
        await automation.build_automation(
            var.get_tap_trigger(),
            [],
            config[CONF_ON_TAP],
        )

    if CONF_ON_DOUBLE_TAP in config:
        await automation.build_automation(
            var.get_double_tap_trigger(),
            [],
            config[CONF_ON_DOUBLE_TAP],
        )

    if CONF_ON_ACTIVE in config:
        await automation.build_automation(
            var.get_active_trigger(),
            [],
            config[CONF_ON_ACTIVE],
        )

    if CONF_ON_FREEFALL in config:
        await automation.build_automation(
            var.get_freefall_trigger(),
            [],
            config[CONF_ON_FREEFALL],
        )
