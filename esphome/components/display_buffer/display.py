import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_FORMAT,
    CONF_WIDTH,
    CONF_HEIGHT,
    CONF_OUTPUT,
)

DEPENDENCIES = ["display"]

display_framebuffer_ns = cg.esphome_ns.namespace("display_buffer")
Buffer = display_framebuffer_ns.class_(
    "Buffer", cg.PollingComponent, display.DisplayBuffer
)

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(Buffer),
            cv.Optional(CONF_WIDTH): cv.int_,
            cv.Optional(CONF_HEIGHT): cv.int_,
            cv.Required(CONF_FORMAT): cv.enum(display.PIXEL_TYPES, upper=True),
            cv.Optional(CONF_OUTPUT): cv.use_id(display.DisplayBuffer),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("5s")),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    cv.has_at_least_one_key(CONF_OUTPUT, CONF_WIDTH),
    cv.has_none_or_all_keys(CONF_WIDTH, CONF_HEIGHT),
)


async def to_code(config):
    template_args = cg.TemplateArguments(config[CONF_FORMAT])
    var = cg.new_Pvariable(config[CONF_ID], template_args)
    cg.add(var.set_width(config[CONF_WIDTH]))
    cg.add(var.set_height(config[CONF_HEIGHT]))
    if CONF_OUTPUT in config:
        cg.add(var.set_output(config[CONF_OUTPUT]))

    await cg.register_component(var, config)
    await display.register_display(var, config)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
