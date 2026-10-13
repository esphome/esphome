from esphome import pins
import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_BRIGHTNESS, CONF_LAMBDA, CONF_MODEL, CONF_RESET_PIN
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@pl4nkton"]
DOMAIN = "sh1122_base"

sh1122_base_ns = cg.esphome_ns.namespace("sh1122_base")
SH1122 = sh1122_base_ns.class_("SH1122", cg.PollingComponent, display.DisplayBuffer)
SH1122Model = sh1122_base_ns.enum("SH1122Model")

MODELS = {
    "SH1122_256X64": SH1122Model.SH1122_MODEL_256_64,
}

SH1122_MODEL = cv.enum(MODELS, upper=True, space="_")

SH1122_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.Optional(CONF_MODEL, default="SH1122_256X64"): SH1122_MODEL,
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
    }
).extend(cv.polling_component_schema("1s"))


async def setup_sh1122(var: MockObj, config: ConfigType) -> None:
    await display.register_display(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))
    if (reset_pin := config.get(CONF_RESET_PIN)) is not None:
        reset = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(reset))
    cg.add(var.init_brightness(config[CONF_BRIGHTNESS]))
    if (lambda_config := config.get(CONF_LAMBDA)) is not None:
        lambda_ = await cg.process_lambda(
            lambda_config, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
