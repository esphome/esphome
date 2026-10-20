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

# The only backpack wired up so far; other HT16K33 boards will join this list.
DEVICE_ADAFRUIT_7_SEG_056IN = "ADAFRUIT_7_SEG_.56IN"

# The chip answers on 0x70 to 0x77, so one bus carries the primary and up to seven more.
MAX_SECONDARY_DISPLAYS = 7
# printf and strftime format into a stack buffer one byte larger, FORMAT_BUFFER_SIZE in ht16k33_char.cpp.
MAX_BUFFER_LENGTH = 64
# The glyph table is indexed with a byte.
MAX_GLYPH_OVERRIDES = 255

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


_SINGLE_CHARACTER = cv.All(
    cv.string_strict, cv.Length(min=1, max=1), _within_glyph_table
)


def single_character(value: object) -> str:
    """A plain function, which the language schema builder expects as a mapping key."""
    return _SINGLE_CHARACTER(value)


SECONDARY_DISPLAY_SCHEMA = cv.Schema({cv.Required(CONF_ADDRESS): cv.i2c_address})


def _validate(config: ConfigType) -> ConfigType:
    addresses = [config[CONF_ADDRESS]] + [
        conf[CONF_ADDRESS] for conf in config.get(CONF_SECONDARY_DISPLAYS, [])
    ]
    if len(set(addresses)) != len(addresses):
        raise cv.Invalid("Every display needs its own address")
    if len(config.get(CONF_ADD_CHARACTERS, {})) > MAX_GLYPH_OVERRIDES:
        raise cv.Invalid(f"At most {MAX_GLYPH_OVERRIDES} characters can be added")
    return config


CONFIG_SCHEMA = cv.All(
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
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x70)),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    capacity = config[CONF_MAX_BUFFER_LENGTH]
    message = f"ht16k33_char_message_{config[CONF_ID]}"
    # Room after the message for the terminator and the bytes the UTF-8 decoder reads past a cut-off sequence.
    cg.add_global(cg.RawStatement(f"static char {message}[{capacity + 4}];"))
    var = cg.new_Pvariable(config[CONF_ID], cg.RawExpression(message), capacity)
    await display.register_display(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_intensity(config[CONF_INTENSITY]))

    for conf in config.get(CONF_SECONDARY_DISPLAYS, []):
        _request_secondary_slot(str(var))
        cg.add(var.add_secondary_display(conf[CONF_ADDRESS]))

    glyphs = {
        ord(char): segments
        for char, segments in config.get(CONF_ADD_CHARACTERS, {}).items()
    }
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
