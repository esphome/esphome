import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_PAGES

CONF_GREYSCALE = "greyscale"


lilygo_t5_47_plus_ns = cg.esphome_ns.namespace("lilygo_t5_47_plus")
LilygoT547PlusDisplay = lilygo_t5_47_plus_ns.class_(
    "LilygoT547PlusDisplay", cg.PollingComponent, display.DisplayBuffer
)

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(LilygoT547PlusDisplay),
            cv.Optional(CONF_GREYSCALE, default=True): cv.boolean,
        }
    ).extend(cv.polling_component_schema("5s")),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    cv.only_with_arduino,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await display.register_display(var, config)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    cg.add(var.set_greyscale(config[CONF_GREYSCALE]))
