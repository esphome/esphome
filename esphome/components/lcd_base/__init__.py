import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_DIMENSIONS, CONF_POSITION

CONF_USER_CHARACTERS = "user_characters"

lcd_base_ns = cg.esphome_ns.namespace("lcd_base")
LCDDisplay = lcd_base_ns.class_("LCDDisplay", cg.PollingComponent)


def validate_lcd_dimensions(value):
    value = cv.dimensions(value)
    if value[0] > 0x40:
        raise cv.Invalid("LCD displays can't have more than 64 columns")
    if value[1] > 4:
        raise cv.Invalid("LCD displays can't have more than 4 rows")
    return value


def validate_user_characters(value):
    positions = set()
    for conf in value:
        if conf[CONF_POSITION] in positions:
            raise cv.Invalid(
                f"Duplicate user defined character at position {conf[CONF_POSITION]}"
            )
        positions.add(conf[CONF_POSITION])
    return value


LCD_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.Required(CONF_DIMENSIONS): validate_lcd_dimensions,
        cv.Optional(CONF_USER_CHARACTERS): cv.All(
            cv.ensure_list(
                cv.Schema(
                    {
                        cv.Required(CONF_POSITION): cv.int_range(min=0, max=7),
                        cv.Required(CONF_DATA): cv.All(
                            cv.ensure_list(cv.int_range(min=0, max=31)),
                            cv.Length(min=8, max=8),
                        ),
                    }
                ),
            ),
            cv.Length(max=8),
            validate_user_characters,
        ),
    }
).extend(cv.polling_component_schema("1s"))


async def setup_lcd_display(var, config):
    await display.register_display(var, config)
    cg.add(var.set_dimensions(config[CONF_DIMENSIONS][0], config[CONF_DIMENSIONS][1]))
    if CONF_USER_CHARACTERS in config:
        for usr in config[CONF_USER_CHARACTERS]:
            cg.add(var.set_user_defined_char(usr[CONF_POSITION], usr[CONF_DATA]))
