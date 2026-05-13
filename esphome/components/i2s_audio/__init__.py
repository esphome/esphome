from dataclasses import dataclass, field

from esphome import pins
import esphome.codegen as cg
from esphome.components.esp32 import (
    add_idf_sdkconfig_option,
    get_esp32_variant,
    include_builtin_idf_component,
)
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import CONF_BITS_PER_SAMPLE, CONF_CHANNEL, CONF_ID, CONF_SAMPLE_RATE
from esphome.core import CORE
from esphome.cpp_generator import MockObjClass
import esphome.final_validate as fv

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32"]
MULTI_CONF = True

CONF_PDM = "pdm"
CONF_ADC_TYPE = "adc_type"

CONF_I2S_DOUT_PIN = "i2s_dout_pin"
CONF_I2S_DIN_PIN = "i2s_din_pin"
CONF_I2S_MCLK_PIN = "i2s_mclk_pin"
CONF_I2S_BCLK_PIN = "i2s_bclk_pin"
CONF_I2S_LRCLK_PIN = "i2s_lrclk_pin"

CONF_I2S_AUDIO = "i2s_audio"
CONF_I2S_AUDIO_ID = "i2s_audio_id"

CONF_I2S_MODE = "i2s_mode"
CONF_PRIMARY = "primary"
CONF_SECONDARY = "secondary"

CONF_USE_APLL = "use_apll"
CONF_BITS_PER_CHANNEL = "bits_per_channel"
CONF_MCLK_MULTIPLE = "mclk_multiple"
CONF_MONO = "mono"
CONF_LEFT = "left"
CONF_RIGHT = "right"
CONF_STEREO = "stereo"
CONF_BOTH = "both"

i2s_audio_ns = cg.esphome_ns.namespace("i2s_audio")
I2SAudioComponent = i2s_audio_ns.class_("I2SAudioComponent", cg.Component)
I2SAudioBase = i2s_audio_ns.class_(
    "I2SAudioBase", cg.Parented.template(I2SAudioComponent)
)
I2SAudioIn = i2s_audio_ns.class_("I2SAudioIn", I2SAudioBase)
I2SAudioOut = i2s_audio_ns.class_("I2SAudioOut", I2SAudioBase)

i2s_mode_t = cg.global_ns.enum("i2s_mode_t")
I2S_MODE_OPTIONS = {
    CONF_PRIMARY: i2s_mode_t.I2S_MODE_MASTER,  # NOLINT
    CONF_SECONDARY: i2s_mode_t.I2S_MODE_SLAVE,  # NOLINT
}

i2s_role_t = cg.global_ns.enum("i2s_role_t")
I2S_ROLE_OPTIONS = {
    CONF_PRIMARY: i2s_role_t.I2S_ROLE_MASTER,  # NOLINT
    CONF_SECONDARY: i2s_role_t.I2S_ROLE_SLAVE,  # NOLINT
}

# https://github.com/espressif/esp-idf/blob/master/components/soc/{variant}/include/soc/soc_caps.h (SOC_I2S_NUM)
I2S_PORTS = {
    VARIANT_ESP32: 2,
    VARIANT_ESP32C3: 1,
    VARIANT_ESP32C5: 1,
    VARIANT_ESP32C6: 1,
    VARIANT_ESP32C61: 1,
    VARIANT_ESP32H2: 1,
    VARIANT_ESP32P4: 3,
    VARIANT_ESP32S2: 1,
    VARIANT_ESP32S3: 2,
}

i2s_channel_fmt_t = cg.global_ns.enum("i2s_channel_fmt_t")
I2S_CHANNELS = {
    CONF_MONO: i2s_channel_fmt_t.I2S_CHANNEL_FMT_ALL_LEFT,  # left data to both channels
    CONF_LEFT: i2s_channel_fmt_t.I2S_CHANNEL_FMT_ONLY_LEFT,  # mono data
    CONF_RIGHT: i2s_channel_fmt_t.I2S_CHANNEL_FMT_ONLY_RIGHT,  # mono data
    CONF_STEREO: i2s_channel_fmt_t.I2S_CHANNEL_FMT_RIGHT_LEFT,  # stereo data to both channels
}

i2s_slot_mode_t = cg.global_ns.enum("i2s_slot_mode_t")
I2S_SLOT_MODE = {
    CONF_MONO: i2s_slot_mode_t.I2S_SLOT_MODE_MONO,
    CONF_STEREO: i2s_slot_mode_t.I2S_SLOT_MODE_STEREO,
}

i2s_std_slot_mask_t = cg.global_ns.enum("i2s_std_slot_mask_t")
I2S_STD_SLOT_MASK = {
    CONF_LEFT: i2s_std_slot_mask_t.I2S_STD_SLOT_LEFT,
    CONF_RIGHT: i2s_std_slot_mask_t.I2S_STD_SLOT_RIGHT,
    CONF_BOTH: i2s_std_slot_mask_t.I2S_STD_SLOT_BOTH,
}

i2s_bits_per_sample_t = cg.global_ns.enum("i2s_bits_per_sample_t")
I2S_BITS_PER_SAMPLE = {
    8: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_8BIT,
    16: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_16BIT,
    24: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_24BIT,
    32: i2s_bits_per_sample_t.I2S_BITS_PER_SAMPLE_32BIT,
}

i2s_bits_per_chan_t = cg.global_ns.enum("i2s_bits_per_chan_t")
I2S_BITS_PER_CHANNEL = {
    "default": i2s_bits_per_chan_t.I2S_BITS_PER_CHAN_DEFAULT,
    8: i2s_bits_per_chan_t.I2S_BITS_PER_CHAN_8BIT,
    16: i2s_bits_per_chan_t.I2S_BITS_PER_CHAN_16BIT,
    24: i2s_bits_per_chan_t.I2S_BITS_PER_CHAN_24BIT,
    32: i2s_bits_per_chan_t.I2S_BITS_PER_CHAN_32BIT,
}

i2s_slot_bit_width_t = cg.global_ns.enum("i2s_slot_bit_width_t")
I2S_SLOT_BIT_WIDTH = {
    "default": i2s_slot_bit_width_t.I2S_SLOT_BIT_WIDTH_AUTO,
    8: i2s_slot_bit_width_t.I2S_SLOT_BIT_WIDTH_8BIT,
    16: i2s_slot_bit_width_t.I2S_SLOT_BIT_WIDTH_16BIT,
    24: i2s_slot_bit_width_t.I2S_SLOT_BIT_WIDTH_24BIT,
    32: i2s_slot_bit_width_t.I2S_SLOT_BIT_WIDTH_32BIT,
}

i2s_mclk_multiple_t = cg.global_ns.enum("i2s_mclk_multiple_t")
I2S_MCLK_MULTIPLE = {
    128: i2s_mclk_multiple_t.I2S_MCLK_MULTIPLE_128,
    256: i2s_mclk_multiple_t.I2S_MCLK_MULTIPLE_256,
    384: i2s_mclk_multiple_t.I2S_MCLK_MULTIPLE_384,
    512: i2s_mclk_multiple_t.I2S_MCLK_MULTIPLE_512,
}

_validate_bits = cv.float_with_unit("bits", "bit")


def validate_mclk_divisible_by_3(config):
    if config[CONF_BITS_PER_SAMPLE] == 24 and config[CONF_MCLK_MULTIPLE] % 3 != 0:
        raise cv.Invalid(
            f"{CONF_MCLK_MULTIPLE} must be divisible by 3 when bits per sample is 24"
        )
    return config


def i2s_audio_component_schema(
    class_: MockObjClass,
    *,
    default_sample_rate: int,
    default_channel: str,
    default_bits_per_sample: str,
):
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.GenerateID(CONF_I2S_AUDIO_ID): cv.use_id(I2SAudioComponent),
            cv.Optional(CONF_CHANNEL, default=default_channel): cv.one_of(
                *I2S_CHANNELS
            ),
            cv.Optional(CONF_SAMPLE_RATE, default=default_sample_rate): cv.int_range(
                min=1
            ),
            cv.Optional(CONF_BITS_PER_SAMPLE, default=default_bits_per_sample): cv.All(
                _validate_bits, cv.one_of(*I2S_BITS_PER_SAMPLE)
            ),
            cv.Optional(CONF_I2S_MODE, default=CONF_PRIMARY): cv.one_of(
                *I2S_MODE_OPTIONS, lower=True
            ),
            cv.Optional(CONF_USE_APLL, default=False): cv.boolean,
            cv.Optional(CONF_MCLK_MULTIPLE, default=256): cv.one_of(*I2S_MCLK_MULTIPLE),
        }
    )


async def register_i2s_audio_component(var, config):
    await cg.register_parented(var, config[CONF_I2S_AUDIO_ID])
    cg.add(var.set_i2s_role(I2S_ROLE_OPTIONS[config[CONF_I2S_MODE]]))
    slot_mode = config[CONF_CHANNEL]
    if slot_mode != CONF_STEREO:
        slot_mode = CONF_MONO
    slot_mask = config[CONF_CHANNEL]
    if slot_mask not in [CONF_LEFT, CONF_RIGHT]:
        slot_mask = CONF_BOTH
    cg.add(var.set_slot_mode(I2S_SLOT_MODE[slot_mode]))
    cg.add(var.set_std_slot_mask(I2S_STD_SLOT_MASK[slot_mask]))
    cg.add(var.set_slot_bit_width(I2S_SLOT_BIT_WIDTH[config[CONF_BITS_PER_SAMPLE]]))
    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_use_apll(config[CONF_USE_APLL]))
    cg.add(var.set_mclk_multiple(I2S_MCLK_MULTIPLE[config[CONF_MCLK_MULTIPLE]]))


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(I2SAudioComponent),
        cv.Optional(CONF_I2S_LRCLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_I2S_BCLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_I2S_MCLK_PIN): pins.internal_gpio_output_pin_number,
    },
)


@dataclass
class I2SAudioData:
    """I2S audio component state stored in CORE.data."""

    port_map: dict[str, int] = field(default_factory=dict)


def _get_data() -> I2SAudioData:
    if CONF_I2S_AUDIO not in CORE.data:
        CORE.data[CONF_I2S_AUDIO] = I2SAudioData()
    return CORE.data[CONF_I2S_AUDIO]


def _assign_ports() -> None:
    """Assign I2S port numbers, prioritizing instances with microphone children.

    Microphones (especially PDM) require port 0 on most ESP32 variants.
    This runs once and stores the mapping in CORE.data.
    """
    data = _get_data()
    if data.port_map:
        return

    full_config = fv.full_config.get()
    i2s_configs = full_config[CONF_I2S_AUDIO]

    # Find i2s_audio instances with microphones that require port 0
    # (PDM and internal ADC only work on I2S port 0)
    port0_parent_id = None
    for mic_config in full_config.get("microphone", []):
        if CONF_I2S_AUDIO_ID not in mic_config:
            continue
        if mic_config.get(CONF_PDM) or mic_config.get(CONF_ADC_TYPE) == "internal":
            if port0_parent_id is not None:
                raise cv.Invalid(
                    "Only one PDM/ADC microphone is supported (requires I2S port 0)"
                )
            port0_parent_id = str(mic_config[CONF_I2S_AUDIO_ID])

    # Assign ports: port 0 parent first (if any), rest get sequential
    next_port = 0
    if port0_parent_id is not None:
        data.port_map[port0_parent_id] = next_port
        next_port += 1
    for config in i2s_configs:
        config_id = str(config[CONF_ID])
        if config_id != port0_parent_id:
            data.port_map[config_id] = next_port
            next_port += 1


def _final_validate(_):
    i2s_audio_configs = fv.full_config.get()[CONF_I2S_AUDIO]
    variant = get_esp32_variant()
    if variant not in I2S_PORTS:
        raise cv.Invalid(f"Unsupported variant {variant}")
    if len(i2s_audio_configs) > I2S_PORTS[variant]:
        raise cv.Invalid(
            f"Only {I2S_PORTS[variant]} I2S audio ports are supported on {variant}"
        )
    _assign_ports()


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Assign I2S port from _final_validate computed mapping
    data = _get_data()
    if (port := data.port_map.get(str(config[CONF_ID]))) is None:
        raise ValueError(f"No I2S port assigned for {config[CONF_ID]}")
    cg.add(var.set_port(port))

    # Re-enable ESP-IDF's I2S driver (excluded by default to save compile time)
    include_builtin_idf_component("esp_driver_i2s")

    # Helps avoid callbacks being skipped due to processor load
    add_idf_sdkconfig_option("CONFIG_I2S_ISR_IRAM_SAFE", True)

    if CONF_I2S_LRCLK_PIN in config:
        cg.add(var.set_lrclk_pin(config[CONF_I2S_LRCLK_PIN]))
    if CONF_I2S_BCLK_PIN in config:
        cg.add(var.set_bclk_pin(config[CONF_I2S_BCLK_PIN]))
    if CONF_I2S_MCLK_PIN in config:
        cg.add(var.set_mclk_pin(config[CONF_I2S_MCLK_PIN]))
