from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CALIBRATION,
    CONF_ID,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_OFFSET_X,
    CONF_OFFSET_Y,
    CONF_OFFSET_Z,
    CONF_RANGE,
    CONF_RESOLUTION,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
)

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["i2c"]


CONF_MSA3XX_ID = "msa3xx_id"

CONF_MIRROR_Z = "mirror_z"
CONF_ON_ACTIVE = "on_active"
CONF_ON_DOUBLE_TAP = "on_double_tap"
CONF_ON_FREEFALL = "on_freefall"
CONF_ON_ORIENTATION = "on_orientation"
CONF_ON_TAP = "on_tap"


msa3xx_ns = cg.esphome_ns.namespace("msa3xx")
MSA3xxComponent = msa3xx_ns.class_(
    "MSA3xxComponent", cg.PollingComponent, i2c.I2CDevice
)

MSAModels = msa3xx_ns.enum("Model", True)
MSA_MODELS = {
    "MSA301": MSAModels.MSA301,
    "MSA311": MSAModels.MSA311,
}

MSARange = msa3xx_ns.enum("Range", True)
MSA_RANGES = {
    "2G": MSARange.RANGE_2G,
    "4G": MSARange.RANGE_4G,
    "8G": MSARange.RANGE_8G,
    "16G": MSARange.RANGE_16G,
}

# MSABandwidth = msa3xx_ns.enum("Bandwidth", True)
# MSA_BANDWIDTHS = {
#   "1.95HZ": MSABandwidth.BW_1_95HZ,
#   "3.9HZ": MSABandwidth.BW_3_9HZ,
#   "7.81HZ": MSABandwidth.BW_7_81HZ,
#   "15.63HZ": MSABandwidth.BW_15_63HZ,
#   "31.25HZ": MSABandwidth.BW_31_25HZ,
#   "62.5HZ": MSABandwidth.BW_62_5HZ,
#   "125HZ": MSABandwidth.BW_125HZ,
#   "250HZ": MSABandwidth.BW_250HZ,
#   "500HZ": MSABandwidth.BW_500HZ,
# }

MSAResolution = msa3xx_ns.enum("Resolution", True)
MSA_RESOLUTIONS = {
    14: MSAResolution.RES_14BIT,
    12: MSAResolution.RES_12BIT,
    10: MSAResolution.RES_10BIT,
    8: MSAResolution.RES_8BIT,
}

CONFIG_SCHEMA = cv.Schema(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MSA3xxComponent),
            cv.Required(CONF_MODEL): cv.enum(MSA_MODELS, upper=True),
            cv.Optional(CONF_RANGE, default="2G"): cv.enum(MSA_RANGES, upper=True),
            #            cv.Optional(CONF_BANDWIDTH, default="250HZ"): cv.enum(MSA_BANDWIDTHS, upper=True),
            cv.Optional(CONF_RESOLUTION): cv.enum(MSA_RESOLUTIONS),
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
            cv.Optional(CONF_ON_DOUBLE_TAP): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_ON_FREEFALL): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_ORIENTATION): automation.validate_automation(
                single=True
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x00))
)


def validate_i2c_address(config):
    if config[CONF_ADDRESS] == 0x00:
        if config[CONF_MODEL] == "MSA301":
            config[CONF_ADDRESS] = 0x26
        elif config[CONF_MODEL] == "MSA311":
            config[CONF_ADDRESS] = 0x62
        return config

    if (config[CONF_MODEL] == "MSA301" and config[CONF_ADDRESS] == 0x26) or (
        config[CONF_MODEL] == "MSA311" and config[CONF_ADDRESS] == 0x62
    ):
        return config

    raise cv.Invalid(
        "Wrong I2C Address ("
        + hex(config[CONF_ADDRESS])
        + "). MSA301 shall be 0x26, MSA311 shall be 0x62."
    )


def validate_resolution(config):
    if CONF_RESOLUTION not in config:
        if config[CONF_MODEL] == "MSA301":
            config[CONF_RESOLUTION] = 14
        elif config[CONF_MODEL] == "MSA311":
            config[CONF_RESOLUTION] = 12
        return config

    if config[CONF_MODEL] == "MSA311" and config[CONF_RESOLUTION] != 12:
        raise cv.Invalid("Check resolution. MSA311 only supports 12-bit")

    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    validate_i2c_address,
    validate_resolution,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_range(MSA_RANGES[config[CONF_RANGE]]))
    cg.add(var.set_resolution(MSA_RESOLUTIONS[config[CONF_RESOLUTION]]))

    if CONF_TRANSFORM in config:
        transform = config[CONF_TRANSFORM]
        cg.add(
            var.set_transform(
                transform[CONF_MIRROR_X],
                transform[CONF_MIRROR_Y],
                transform[CONF_MIRROR_Z],
                transform[CONF_SWAP_XY],
            )
        )

    if CONF_CALIBRATION in config:
        calibration_config = config[CONF_CALIBRATION]
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
