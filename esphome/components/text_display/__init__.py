from typing import Any

import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_DIMENSIONS
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@tomaszduda23"]
DOMAIN = "text_display"

TextDisplay = cg.esphome_ns.namespace("text_display").class_(
    "TextDisplay", cg.PollingComponent
)


def validate_dimensions(value: Any) -> list[int]:
    value = cv.dimensions(value)
    if value[0] > 255 or value[1] > 255:
        raise cv.Invalid("A text display can have at most 255 columns and 255 rows")
    return value


TEXT_DISPLAY_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.Required(CONF_DIMENSIONS): validate_dimensions,
    }
)


async def setup_text_display(var: MockObj, config: ConfigType) -> None:
    await display.register_display(var, config)
    columns, rows = config[CONF_DIMENSIONS]
    cg.add(var.set_dimensions(columns, rows))
