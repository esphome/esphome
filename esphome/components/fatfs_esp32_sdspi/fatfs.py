import esphome.codegen as cg
from esphome.components import fatfs_esp32, spi
import esphome.config_validation as cv

CODEOWNERS = ["@abel-msk"]
AUTO_LOAD = ["fatfs_esp32"]
DEPENDENCIES = ["spi"]

FatESP32sdspi_ns = cg.esphome_ns.namespace("fatfs_esp32_sdspi")
FatESP32sdspi = FatESP32sdspi_ns.class_(
    "FatESP32sdspi", fatfs_esp32.FatESP32, cg.PollingComponent
)


CONFIG_SCHEMA = (
    fatfs_esp32.fatfs_esp32_schema(FatESP32sdspi)
    .extend(spi.spi_device_schema(cs_pin_required=True, default_mode="MODE0"))
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config):
    cg.add_build_flag("-DCONFIG_FATFS_API_ENCODING_UTF_8")
    cg.add_build_flag("-DCONFIG_FATFS_MAX_LFN=254")
    # cg.add_build_flag("-DCONFIG_FATFS_LFN_STACK")
    cg.add_build_flag("-DCONFIG_FATFS_LFN_HEAP")
    var = await fatfs_esp32.new_esp32_driver(config)
    await spi.register_spi_device(var, config)
    await cg.register_component(var, config)
