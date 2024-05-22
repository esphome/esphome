import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    CONF_RANGE,
)

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor"]

CONF_MSA3XX_ID = "msa3xx_id"

CONF_OFFSET_X = "offset_x"
CONF_OFFSET_Y = "offset_y"
CONF_OFFSET_Z = "offset_z"
CONF_ON_TAP = "on_tap"
CONF_ON_DOUBLE_TAP = "on_double_tap"
CONF_ON_FREEFALL = "on_freefall"
CONF_ON_ORIENTATION = "on_orientation"


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
MSA_RANGE = {
    "2G": MSARange.RANGE_2G,
    "4G": MSARange.RANGE_4G,
    "8G": MSARange.RANGE_8G,
    "16G": MSARange.RANGE_16G,
}


CONFIG_SCHEMA = cv.Schema(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MSA3xxComponent),
            cv.Required(CONF_MODEL): cv.enum(MSA_MODELS, upper=True),
            cv.Optional(CONF_RANGE, default="2G"): cv.enum(MSA_RANGE, upper=True),
            cv.Optional(CONF_OFFSET_X, default=0): cv.float_range(min=-4.5, max=4.5),
            cv.Optional(CONF_OFFSET_Y, default=0): cv.float_range(min=-4.5, max=4.5),
            cv.Optional(CONF_OFFSET_Z, default=0): cv.float_range(min=-4.5, max=4.5),
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
    .extend(i2c.i2c_device_schema(0x62))
)


# def validate_model(config):
#     model = config[CONF_MODEL]


# FINAL_VALIDATE_SCHEMA = validate_model


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(
        var.set_offset(
            config[CONF_OFFSET_X], config[CONF_OFFSET_Y], config[CONF_OFFSET_Z]
        )
    )
    cg.add(var.set_range(config[CONF_RANGE]))

    irq_set_0 = 0
    irq_set_1 = 0

    if CONF_ON_TAP in config:
        await automation.build_automation(
            var.get_tap_trigger(),
            [],
            config[CONF_ON_TAP],
        )
        irq_set_0 |= 1 << 5

    if CONF_ON_DOUBLE_TAP in config:
        await automation.build_automation(
            var.get_double_tap_trigger(),
            [],
            config[CONF_ON_DOUBLE_TAP],
        )
        irq_set_0 |= 1 << 4

    if CONF_ON_ORIENTATION in config:
        await automation.build_automation(
            var.get_orientation_trigger(),
            [],
            config[CONF_ON_ORIENTATION],
        )
        irq_set_0 |= 1 << 6

    if CONF_ON_FREEFALL in config:
        await automation.build_automation(
            var.get_freefall_trigger(),
            [],
            config[CONF_ON_FREEFALL],
        )
        irq_set_1 |= 1 << 3

    cg.add(var.set_interrupts(irq_set_0, irq_set_1))
