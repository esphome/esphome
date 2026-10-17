import esphome.codegen as cg
from esphome.components import audio_dac, i2c
import esphome.config_validation as cv
from esphome.const import CONF_BITS_PER_SAMPLE, CONF_ID, CONF_SAMPLE_RATE
from esphome.types import ConfigType

CONF_DEEMPHASIS = "deemphasis"
CONF_DOP = "dop"
CONF_FILTER_SHAPE = "filter_shape"

# Fields of the Katana format register: channel layout, word length and rate code.
FORMAT_STEREO = 0x80
BITS_PER_SAMPLE_CODES = {16: 0x10, 24: 0x20, 32: 0x30}
SAMPLE_RATE_CODES = {
    44100: 0x04,
    48000: 0x05,
    88200: 0x06,
    96000: 0x07,
    176400: 0x08,
    192000: 0x09,
    352800: 0x0A,
    384000: 0x0B,
}

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
DEEMPHASIS_MODES = DeemphasisMode.options(
    "bypass", "32khz", "44_1khz", "48khz", prefix="DEEMPHASIS_"
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES9038Q2MKatana),
            cv.Optional(CONF_FILTER_SHAPE, default="apodizing"): cv.enum(
                FILTER_SHAPES, lower=True
            ),
            cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): cv.All(
                cv.float_with_unit("bits", "bit"),
                cv.one_of(*BITS_PER_SAMPLE_CODES, int=True),
            ),
            cv.Optional(CONF_SAMPLE_RATE, default=48000): cv.one_of(
                *SAMPLE_RATE_CODES, int=True
            ),
            cv.Optional(CONF_DEEMPHASIS, default="bypass"): cv.enum(
                DEEMPHASIS_MODES, lower=True
            ),
            # Only switches the control logic; the source must already send DoP frames.
            cv.Optional(CONF_DOP, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x30))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_filter_shape(config[CONF_FILTER_SHAPE]))
    cg.add(
        var.set_format(
            FORMAT_STEREO
            | BITS_PER_SAMPLE_CODES[config[CONF_BITS_PER_SAMPLE]]
            | SAMPLE_RATE_CODES[config[CONF_SAMPLE_RATE]]
        )
    )
    cg.add(var.set_deemphasis_mode(config[CONF_DEEMPHASIS]))
    cg.add(var.set_dop_enabled(config[CONF_DOP]))
