# from esphome import automation, pins
# import logging
from esphome import automation, pins
import esphome.codegen as cg

# from esphome.components.esp32.const import KEY_ESP32, VARIANT_ESP32, VARIANT_ESP32S3
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PATH
from esphome.cpp_generator import MockObjClass

# from esphome.cpp_generator import MockObjClass

MOUNT_POINT = "mount_point"
CONF_DRIVE_ID = "drive_id"
CONF_CD_PIN = "cd_pin"

IS_PLATFORM_COMPONENT = True
CODEOWNERS = ["@abel-msk"]
# _LOGGER = logging.getLogger(__name__)

fatfs_ns = cg.esphome_ns.namespace("fatfs")
FatFS = fatfs_ns.class_("FatFS", cg.PollingComponent)

FatIsExistCondition = fatfs_ns.class_(
    "FatIsExistCondition", automation.Condition.template()
)

#
#   Validators
#

# def validate_id(config):
#     if CONF_DRIVE_ID in config:
#         drv_id = config[CONF_DRIVE_ID]
#         if drv_id > 10:
#             raise cv.Invalid("Use Drive id in range 0 - 10")
#     return config


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


#
#   Schemas
#

FATFS_SCHEMA_ = cv.Schema(
    {
        # cv.GenerateID(): cv.declare_id(FatFS),
        cv.Optional(CONF_DRIVE_ID): cv.int_range(min=0, max=10),
        cv.Optional(CONF_CD_PIN): pins.internal_gpio_output_pin_number,
    },
)
# FATFS_SCHEMA_.add_extra(validate_id)


def fatfs_schema(
    class_: MockObjClass,
) -> cv.Schema:
    schema = {
        cv.GenerateID(): cv.declare_id(class_),
    }
    return FATFS_SCHEMA_.extend(schema)


#
#   To code
#


async def setup_fat_core_(var, config):
    # await cg.register_component(var, config)
    # cg.add(var.set_drive_id([config[CONF_DRIVE_ID]]))
    if CONF_DRIVE_ID in config:
        cg.add(var.set_drive_id(config[CONF_DRIVE_ID]))
    if CONF_CD_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_CD_PIN])
        cg.add(var.set_cd_pin(pin))


async def new_fatfs(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await setup_fat_core_(var, config)
    return var


async def to_code(config):
    raise NotImplementedError(
        "This component if only FATFS API. You need fatfs component with device driver."
    )


#
#   Triggers
#


@automation.register_condition(
    "fatfs.is_exist",
    FatIsExistCondition,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(FatFS),
            cv.Optional(CONF_PATH, default="/"): cv.templatable(cv.string),
        }
    ),
)
async def fat_is_exist_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(condition_id, template_arg, paren)
    path = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(path))
    return var
