import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.esp32 import VARIANT_ESP32S3, only_on_variant
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_MODEL
from esphome.final_validate import full_config

DEPENDENCIES = ["esp32", "spi"]

CONF_VCOM = "vcom"

it8951_ns = cg.esphome_ns.namespace("it8951")
IT8951Display = it8951_ns.class_("IT8951Display", display.DisplayBuffer, spi.SPIDevice)
IT8951Model = it8951_ns.enum("IT8951Model")

MODELS = {
    "seeed-reterminal-e1003": IT8951Model.IT8951_MODEL_SEEED_RETERMINAL_E1003,
    "seeed-ee03": IT8951Model.IT8951_MODEL_SEEED_EE03,
}


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(IT8951Display),
            cv.Required(CONF_MODEL): cv.enum(MODELS, lower=True, space="-"),
            cv.Optional(CONF_VCOM, default=1400): cv.int_range(min=0, max=5000),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema(True, "1MHz", "MODE0")),
    cv.only_on_esp32,
    only_on_variant(supported=[VARIANT_ESP32S3]),
)


def _final_validate(config):
    if "psram" not in full_config.get():
        raise cv.Invalid("The it8951 display component requires PSRAM to be enabled")
    return spi.final_validate_device_schema(
        "it8951", require_miso=True, require_mosi=True
    )(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await spi.register_spi_device(var, config)
    await display.register_display(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_vcom(config[CONF_VCOM]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
