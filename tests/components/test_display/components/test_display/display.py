import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_DIMENSIONS, CONF_HEIGHT, CONF_ID, CONF_WIDTH
from esphome.core import CoroPriority, coroutine_with_priority

test_display_ns = cg.esphome_ns.namespace("test_display")
TestDisplay = test_display_ns.class_("TestDisplay", display.Display)

CONFIG_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TestDisplay),
        cv.Optional(CONF_DIMENSIONS, default="100x100"): cv.Any(
            cv.dimensions,
            cv.Schema(
                {
                    cv.Required(CONF_WIDTH): cv.int_,
                    cv.Required(CONF_HEIGHT): cv.int_,
                }
            ),
        ),
    }
)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    dimensions = config[CONF_DIMENSIONS]
    if isinstance(dimensions, dict):
        width, height = dimensions[CONF_WIDTH], dimensions[CONF_HEIGHT]
    else:
        width, height = dimensions
    cg.add(var.set_dimensions(width, height))
