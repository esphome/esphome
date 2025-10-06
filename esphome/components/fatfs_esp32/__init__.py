# from esphome import automation, pins
# import logging
import esphome.codegen as cg
from esphome.components import fatfs

# from esphome.components.esp32.const import KEY_ESP32, VARIANT_ESP32, VARIANT_ESP32S3
import esphome.config_validation as cv
from esphome.const import PLATFORM_ESP32
from esphome.core import CORE
from esphome.cpp_generator import MockObjClass

# from esphome.cpp_generator import MockObjClass

# MOUNT_POINT = "munt_point"
# CONF_DRIVE_ID = "drive_id"

# IS_PLATFORM_COMPONENT = True
CODEOWNERS = ["@abel-msk"]
# _LOGGER = logging.getLogger(__name__)

FatESP32_ns = cg.esphome_ns.namespace("fatfs_esp32")
# FatESP32 = FatESP32_ns.class_("FatESP32", fatfs.FatFS, cg.PollingComponent)
FatESP32 = FatESP32_ns.class_("FatESP32", fatfs.FatFS)

#
#   Validators
#


def validate_esp32(config):
    if CORE.target_platform != PLATFORM_ESP32:
        raise cv.Invalid("This platform can be used only with ESP32 any variants.")


#
#   Schemas
#


def fatfs_esp32_schema(
    class_: MockObjClass,
) -> cv.Schema:
    return fatfs.fatfs_schema(class_)


# .add_extra(validate_esp32)

#
#   To code
#
# async def register_esp32_driver(var, config):
#     await cg.register_component(var, config)


async def new_esp32_driver(config, *args):
    return await fatfs.new_fatfs(config, *args)
    # await register_esp32_driver(var, config)
    # return var
