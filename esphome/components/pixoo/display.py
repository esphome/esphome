import esphome.codegen as cg
from esphome.components import display, spi
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_MODEL
from esphome.types import ConfigType

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["split_buffer"]

CONF_PIXOO_ID = "pixoo_id"

pixoo_ns = cg.esphome_ns.namespace("pixoo")
Pixoo = pixoo_ns.class_("Pixoo", cg.PollingComponent, display.Display, spi.SPIDevice)
PixooModel = pixoo_ns.enum("PixooModel")

# Only the 64x64 panel is hardware-verified. Smaller Pixoo panels are assumed to share the
# same protocol; add them here once confirmed.
MODELS = {
    "64X64": PixooModel.PIXOO_64,
}

CONFIG_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(Pixoo),
        cv.Optional(CONF_MODEL, default="64X64"): cv.enum(MODELS, upper=True),
    }
).extend(spi.spi_device_schema(cs_pin_required=True, default_data_rate=8e6))

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "pixoo", require_miso=False, require_mosi=True
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_MODEL])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config, write_only=True)

    if (lambda_config := config.get(CONF_LAMBDA)) is not None:
        lambda_ = await cg.process_lambda(
            lambda_config, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
