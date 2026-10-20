import esphome.codegen as cg
from esphome.components import display, i2c
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_DEVICE, CONF_ID, CONF_INTENSITY
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["unicode"]

CONF_MAX_BUFFER_LENGTH = "max_buffer_length"
CONF_SECONDARY_DISPLAYS = "secondary_displays"
CONF_ADD_CHARACTERS = "add_characters"
CONF_REMOVE_CHARACTERS = "remove_characters"

# The only backpack wired up so far; other HT16K33 boards will join this list.
DEVICE_ADAFRUIT_7_SEG_056IN = "ADAFRUIT_7_SEG_.56IN"

# The chip answers on 0x70 to 0x77, so one bus carries the primary and up to seven more.
MAX_SECONDARY_DISPLAYS = 7
# Must match FORMAT_BUFFER_SIZE in ht16k33_char.cpp, the stack buffer printf and strftime format into.
MAX_BUFFER_LENGTH = 64

ht16k33_char_ns = cg.esphome_ns.namespace("ht16k33_char")
HT16K33CharDisplay = ht16k33_char_ns.class_(
    "HT16K33CharDisplay", cg.PollingComponent, i2c.I2CDevice
)
HT16K33CharDisplayRef = HT16K33CharDisplay.operator("ref")
Glyph = ht16k33_char_ns.struct("Glyph")

_request_secondary_slot = cg.slot_counter("HT16K33_CHAR_SECONDARY_COUNT")


def _within_glyph_table(value: str) -> str:
    if ord(value) > 0xFFFF:
        raise cv.Invalid("Characters above U+FFFF are not supported")
    return value


single_character = cv.All(
    cv.string_strict, cv.Length(min=1, max=1), _within_glyph_table
)


SECONDARY_DISPLAY_SCHEMA = cv.Schema({cv.Required(CONF_ADDRESS): cv.i2c_address})

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HT16K33CharDisplay),
            cv.Required(CONF_DEVICE): cv.one_of(
                DEVICE_ADAFRUIT_7_SEG_056IN, upper=True
            ),
            cv.Optional(CONF_MAX_BUFFER_LENGTH, default=8): cv.int_range(
                min=4, max=MAX_BUFFER_LENGTH
            ),
            cv.Optional(CONF_INTENSITY, default=15): cv.int_range(min=0, max=15),
            cv.Optional(CONF_SECONDARY_DISPLAYS): cv.All(
                cv.ensure_list(SECONDARY_DISPLAY_SCHEMA),
                cv.Length(max=MAX_SECONDARY_DISPLAYS),
            ),
            cv.Optional(CONF_ADD_CHARACTERS): cv.Schema(
                {single_character: cv.hex_uint8_t}
            ),
            cv.Optional(CONF_REMOVE_CHARACTERS): cv.ensure_list(single_character),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x70))
)


async def to_code(config: ConfigType) -> None:
    capacity = config[CONF_MAX_BUFFER_LENGTH]
    message = f"{config[CONF_ID]}_message"
    # One byte more than the message, for the terminator the UTF-8 decoder stops at.
    cg.add_global(cg.RawStatement(f"static char {message}[{capacity + 1}];"))
    var = cg.new_Pvariable(config[CONF_ID], cg.RawExpression(message), capacity)
    await display.register_display(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_intensity(config[CONF_INTENSITY]))

    for conf in config.get(CONF_SECONDARY_DISPLAYS, []):
        _request_secondary_slot(str(var))
        cg.add(var.add_secondary_display(conf[CONF_ADDRESS]))

    # A removed character draws as a blank digit, which is what segments 0 means.
    glyphs = {
        ord(char): segments
        for char, segments in config.get(CONF_ADD_CHARACTERS, {}).items()
    }
    glyphs.update({ord(char): 0 for char in config.get(CONF_REMOVE_CHARACTERS, [])})
    if glyphs:
        table = cg.shared_progmem_array(
            "ht16k33_char_glyphs",
            Glyph,
            [
                cg.RawExpression(f"{{{codepoint}, {segments}}}")
                for codepoint, segments in glyphs.items()
            ],
        )
        cg.add_define("USE_HT16K33_CHAR_GLYPH_OVERRIDES")
        cg.add(var.set_glyph_overrides(table, len(glyphs)))

    await display.add_writer(var, config, HT16K33CharDisplayRef)
