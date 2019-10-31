import math
import re

from esphome import pins, automation
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_BIT_DEPTH, CONF_CHANNEL, CONF_FREQUENCY, \
    CONF_ID, CONF_PIN, ESP_PLATFORM_ESP32, CONF_DURATION

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]


def calc_max_frequency(bit_depth):
    return 80e6 / (2**bit_depth)


def calc_min_frequency(bit_depth):
    max_div_num = ((2**20) - 1) / 256.0
    return 80e6 / (max_div_num * (2**bit_depth))


def validate_frequency(value):
    value = cv.frequency(value)
    min_freq = calc_min_frequency(20)
    max_freq = calc_max_frequency(1)
    if value < min_freq:
        raise cv.Invalid("This frequency setting is not possible, please choose a higher "
                         "frequency (at least {}Hz)".format(int(min_freq)))
    if value > max_freq:
        raise cv.Invalid("This frequency setting is not possible, please choose a lower "
                         "frequency (at most {}Hz)".format(int(max_freq)))
    return value


ledc_ns = cg.esphome_ns.namespace('ledc')
LEDCOutput = ledc_ns.class_('LEDCOutput', output.FloatOutput, cg.Component)
SetFrequencyAction = ledc_ns.class_('SetFrequencyAction', automation.Action)
SongAction = ledc_ns.class_('SongAction', automation.Action, cg.Component)
SongActionItem = ledc_ns.class_('SongActionItem')

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(LEDCOutput),
    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_FREQUENCY, default='1kHz'): cv.frequency,
    cv.Optional(CONF_CHANNEL): cv.int_range(min=0, max=15),

    cv.Optional(CONF_BIT_DEPTH): cv.invalid("The bit_depth option has been removed in v1.14, the "
                                            "best bit depth is now automatically calculated."),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    gpio = yield cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], gpio)
    yield cg.register_component(var, config)
    yield output.register_output(var, config)
    if CONF_CHANNEL in config:
        cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))


@automation.register_action('output.ledc.set_frequency', SetFrequencyAction, cv.Schema({
    cv.Required(CONF_ID): cv.use_id(LEDCOutput),
    cv.Required(CONF_FREQUENCY): cv.templatable(validate_frequency),
}))
def ledc_set_frequency_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_FREQUENCY], args, float)
    cg.add(var.set_frequency(template_))
    yield var


NOTES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
NOTE_PATTERN = r'^([{}])\s*([0-9]*)$'.format(r'|'.join(NOTES))


def validate_note(value):
    value = cv.string(value).upper()
    match = re.match(NOTE_PATTERN, value)
    if match is None:
        raise cv.Invalid("Invalid note '{}'. Please make sure the format is like "
                         "'A' (default octave 4), or 'G#4'.")
    note = NOTES.index(match.group(1))
    octave = int(match.group(2) or 4)
    return '{}{}'.format(NOTES[note], octave)


def note_to_freq(value):
    match = re.match(NOTE_PATTERN, value)
    note = NOTES.index(match.group(1))
    octave = int(match.group(2))
    key = note + octave * len(NOTES)

    a4 = 440
    base = math.pow(2, 1/12)
    c0 = a4 * math.pow(base, -57)

    # https://pages.mtu.edu/~suits/NoteFreqCalcs.html
    # f_n = f_0 * a^n
    # f_0 ... the frequency of the fixed note. Here: C0
    # n   ... the number of half steps away from the fixed note
    # f_n ... frequency of the note `n` half steps away
    # a   ... the twelfth root of 2

    return c0 * math.pow(base, key)


CONF_NOTE = 'note'
CONF_SPACE = 'space'
CONF_ACTIVE_LEVEL = 'active_level'
CONF_PATTERN = 'pattern'


def validate_pattern_item(value):
    if not isinstance(value, dict):
        raise cv.Invalid("Pattern item needs to be a key-value mapping. But this is a {}"
                         "".format(type(value)))

    # pylint: disable=no-else-return
    if CONF_FREQUENCY in value:
        return cv.Schema({
            cv.Required(CONF_FREQUENCY): cv.frequency,
            cv.Required(CONF_DURATION): cv.positive_time_period_milliseconds,
        })(value)
    elif CONF_NOTE in value:
        # Convert to frequency representation
        value = cv.Schema({
            cv.Required(CONF_NOTE): validate_note,
            cv.Required(CONF_DURATION): cv.positive_time_period_milliseconds,
        })(value)
        return validate_pattern_item({
            CONF_FREQUENCY: note_to_freq(value[CONF_NOTE]),
            CONF_DURATION: value[CONF_DURATION]
        })
    elif CONF_SPACE in value:
        # Convert to frequency 0
        value = cv.Schema({
            cv.Required(CONF_SPACE): cv.positive_time_period_milliseconds,
        })(value)
        return validate_pattern_item({
            CONF_FREQUENCY: 0,
            CONF_DURATION: value[CONF_SPACE]
        })

    raise cv.Invalid("At least one key of [frequency], [note], [space] is required!")


@automation.register_action('output.ledc.song', SongAction, cv.Schema({
    cv.Required(CONF_ID): cv.use_id(LEDCOutput),
    cv.Required(CONF_PATTERN): [validate_pattern_item],
    cv.Optional(CONF_ACTIVE_LEVEL, default='50%'): cv.percentage,
}).extend(cv.COMPONENT_SCHEMA))
def ledc_song_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    yield cg.register_component(var, config)
    cg.add(var.set_active_level(config[CONF_ACTIVE_LEVEL]))

    items = []
    end_at = 0
    for conf in config[CONF_PATTERN]:
        end_at += conf[CONF_DURATION].total_milliseconds
        items.append(SongActionItem(conf[CONF_FREQUENCY], end_at))
    cg.add(var.set_items(items))

    yield var
