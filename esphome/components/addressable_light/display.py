import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, light
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_ADDRESSABLE_LIGHT_ID,
    CONF_HEIGHT,
    CONF_WIDTH,
    CONF_UPDATE_INTERVAL,
    CONF_PIXEL_MAPPER,
)

CODEOWNERS = ["@justfalter"]

addressable_light_ns = cg.esphome_ns.namespace("addressable_light")
AddressableLightDisplay = addressable_light_ns.class_(
    "AddressableLightDisplay", display.DisplayBuffer, cg.PollingComponent
)

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(AddressableLightDisplay),
            cv.Required(CONF_ADDRESSABLE_LIGHT_ID): cv.use_id(
                light.AddressableLightState
            ),
            cv.Required(CONF_WIDTH): cv.positive_int,
            cv.Required(CONF_HEIGHT): cv.positive_int,
            cv.Optional(
                CONF_UPDATE_INTERVAL, default="16ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PIXEL_MAPPER): cv.returning_lambda,
        }
    ),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    wrapped_light = await cg.get_variable(config[CONF_ADDRESSABLE_LIGHT_ID])
    cg.add(var.set_width(config[CONF_WIDTH]))
    cg.add(var.set_height(config[CONF_HEIGHT]))
    cg.add(var.set_light(wrapped_light))

    await cg.register_component(var, config)
    await display.register_display(var, config)

    if pixel_mapper := config.get(CONF_PIXEL_MAPPER):
        pixel_mapper_template_ = await cg.process_lambda(
            pixel_mapper,
            [(int, "x"), (int, "y")],
            return_type=cg.int_,
        )
        cg.add(var.set_pixel_mapper(pixel_mapper_template_))

    if lambda_config := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lambda_config, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
