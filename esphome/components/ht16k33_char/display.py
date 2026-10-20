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
CONF_MESSAGE_ID = "message_id"

# The only board so far
DEVICE_ADAFRUIT_7_SEG_056IN = "ADAFRUIT_7_SEG_.56IN"

# Addresses 0x70 to 0x77: the primary plus seven
MAX_SECONDARY_DISPLAYS = 7
# FORMAT_BUFFER_SIZE in ht16k33_char.cpp is one more
MAX_BUFFER_LENGTH = 64

ht16k33_char_ns = cg.esphome_ns.namespace("ht16k33_char")
HT16K33CharDisplay = ht16k33_char_ns.class_(
    "HT16K33CharDisplay", cg.PollingComponent, i2c.I2CDevice
)
HT16K33CharDisplayRef = HT16K33CharDisplay.operator("ref")

_request_secondary_slot = cg.slot_counter("HT16K33_CHAR_SECONDARY_COUNT")


def _within_glyph_table(value: str) -> str:
    # 0 ends the table and can never be printed
    if not 0 < ord(value) <= 0xFFFF:
        raise cv.Invalid("Only characters from U+0001 to U+FFFF are supported")
    return value


_SINGLE_CHARACTER = cv.All(
    cv.string_strict, cv.Length(min=1, max=1), _within_glyph_table
)


def single_character(value: object) -> str:
    """The schema builder needs a plain function as a mapping key."""
    return _SINGLE_CHARACTER(value)


SECONDARY_DISPLAY_SCHEMA = cv.Schema({cv.Required(CONF_ADDRESS): cv.i2c_address})


def _validate(config: ConfigType) -> ConfigType:
    addresses = [config[CONF_ADDRESS]] + [
        conf[CONF_ADDRESS] for conf in config.get(CONF_SECONDARY_DISPLAYS, [])
    ]
    if len(set(addresses)) != len(addresses):
        raise cv.Invalid("Every display needs its own address")
    return config


CONFIG_SCHEMA = cv.All(
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HT16K33CharDisplay),
            cv.GenerateID(CONF_MESSAGE_ID): cv.declare_id(cg.char),
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
    message = config[CONF_MESSAGE_ID].id
    # Terminator plus the bytes the UTF-8 decoder may read past a cut-off sequence
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
        # One word per entry, code point low and segments high; 0 ends the table
        table = cg.shared_progmem_array(
            "ht16k33_char_glyphs",
            cg.uint32,
            [codepoint | segments << 16 for codepoint, segments in glyphs.items()]
            + [0],
        )
        cg.add_define("USE_HT16K33_CHAR_GLYPH_OVERRIDES")
        cg.add(var.set_glyph_overrides(table))

    await display.add_writer(var, config, HT16K33CharDisplayRef)
