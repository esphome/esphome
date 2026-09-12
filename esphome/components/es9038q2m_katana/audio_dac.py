import esphome.codegen as cg
from esphome.components import audio_dac, i2c
import esphome.config_validation as cv
from esphome.const import CONF_BITS_PER_SAMPLE, CONF_ID, CONF_SAMPLE_RATE

CONF_DEEMPHASIS = "deemphasis"
CONF_DOP = "dop"
CONF_DUMP_REGISTERS = "dump_registers"
CONF_FILTER_SHAPE = "filter_shape"

BITS_PER_SAMPLE_OPTIONS = {
    "16bit": 16,
    "24bit": 24,
    "32bit": 32,
}

# Sample rates exposed by the Katana control interface.
# Keep the list explicit so unsupported values fail validation early.
SUPPORTED_SAMPLE_RATES = [
    44100,
    48000,
    88200,
    96000,
    176400,
    192000,
    352800,
    384000,
]

es9038q2m_katana_ns = cg.esphome_ns.namespace("es9038q2m_katana")
ES9038Q2MKatana = es9038q2m_katana_ns.class_(
    "ES9038Q2MKatana",
    audio_dac.AudioDac,
    cg.Component,
    i2c.I2CDevice,
)

FilterShape = es9038q2m_katana_ns.enum("FilterShape")
FILTER_SHAPES = {
    "linear_phase_fast": FilterShape.FILTER_SHAPE_LINEAR_PHASE_FAST,
    "linear_phase_slow": FilterShape.FILTER_SHAPE_LINEAR_PHASE_SLOW,
    "minimum_phase_fast": FilterShape.FILTER_SHAPE_MIN_PHASE_FAST,
    "minimum_phase_slow": FilterShape.FILTER_SHAPE_MIN_PHASE_SLOW,
    "apodizing": FilterShape.FILTER_SHAPE_APODIZING,
    "hybrid": FilterShape.FILTER_SHAPE_HYBRID,
    "brick_wall": FilterShape.FILTER_SHAPE_BRICK_WALL,
}

DeemphasisMode = es9038q2m_katana_ns.enum("DeemphasisMode")
DEEMPHASIS_MODES = {
    "bypass": DeemphasisMode.DEEMPHASIS_BYPASS,
    "32khz": DeemphasisMode.DEEMPHASIS_32KHZ,
    "44_1khz": DeemphasisMode.DEEMPHASIS_44_1KHZ,
    "48khz": DeemphasisMode.DEEMPHASIS_48KHZ,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES9038Q2MKatana),
            cv.Optional(CONF_FILTER_SHAPE, default="apodizing"): cv.enum(
                FILTER_SHAPES, lower=True
            ),
            # Use a conservative PCM default for initial bring-up.
            cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): cv.enum(
                BITS_PER_SAMPLE_OPTIONS, lower=True
            ),
            cv.Optional(CONF_SAMPLE_RATE, default=48000): cv.one_of(
                *SUPPORTED_SAMPLE_RATES, int=True
            ),
            # De-emphasis only matters for legacy pre-emphasized PCM material.
            # Leave it at "bypass" for normal playback.
            cv.Optional(CONF_DEEMPHASIS, default="bypass"): cv.enum(
                DEEMPHASIS_MODES, lower=True
            ),
            # This only enables DoP mode on the control side.
            # The audio source still needs to send valid DoP frames.
            cv.Optional(CONF_DOP, default=False): cv.boolean,
            # Helpful during bring-up, but left opt-in to keep logs compact.
            cv.Optional(CONF_DUMP_REGISTERS, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x30))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # This component configures the Katana control interface, not the raw DAC registers.
    cg.add(var.set_filter_shape(config[CONF_FILTER_SHAPE]))
    cg.add(var.set_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_deemphasis_mode(config[CONF_DEEMPHASIS]))
    cg.add(var.set_dop_enabled(config[CONF_DOP]))
    cg.add(var.set_dump_registers(config[CONF_DUMP_REGISTERS]))
