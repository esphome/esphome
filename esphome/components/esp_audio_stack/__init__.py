"""ESP Audio Stack Component - full-duplex I2S for simultaneous mic and speaker.

Exposes standard ESPHome microphone and speaker platforms for compatibility with
Voice Assistant, wake word, media playback and custom audio consumers.

Multi-rate support: set output_sample_rate to convert mic audio internally.
  sample_rate: I2S bus rate (e.g. 48000 for high-quality DAC output)
  output_sample_rate: mic/AEC/MWW/VA rate (e.g. 16000, must divide sample_rate evenly)
If output_sample_rate is omitted, no rate conversion occurs (backward compatible).
"""

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import i2c, psram
from esphome.components.esp32 import (
    add_idf_component,
    add_idf_sdkconfig_option,
    get_esp32_variant,
    include_builtin_idf_component,
)
from esphome.components.esp32.const import VARIANT_ESP32P4, VARIANT_ESP32S3
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_BITS_PER_SAMPLE,
    CONF_I2C_ID,
    CONF_ID,
    CONF_INPUT,
    CONF_NUM_CHANNELS,
    CONF_ON_IDLE,
    CONF_ON_START,
    CONF_ON_STATE,
    CONF_OUTPUT,
    CONF_SAMPLE_RATE,
    CONF_TYPE,
    Framework,
)

CODEOWNERS = ["@n-IA-hane"]
DEPENDENCIES = ["esp32"]

CONF_I2S_LRCLK_PIN = "i2s_lrclk_pin"
CONF_I2S_BCLK_PIN = "i2s_bclk_pin"
CONF_I2S_MCLK_PIN = "i2s_mclk_pin"
CONF_I2S_DIN_PIN = "i2s_din_pin"
CONF_I2S_DOUT_PIN = "i2s_dout_pin"
CONF_OUTPUT_SAMPLE_RATE = "output_sample_rate"
CONF_PROCESSOR_ID = "processor_id"
CONF_INPUT_GAIN = "input_gain"
CONF_MASTER_VOLUME_MIN_DB = "master_volume_min_db"
CONF_USE_STEREO_AEC_REFERENCE = "use_stereo_aec_reference"
CONF_REFERENCE_CHANNEL = "reference_channel"
CONF_SLOT_BIT_WIDTH = "slot_bit_width"
CONF_CORRECT_DC_OFFSET = "correct_dc_offset"
CONF_MIC_CHANNEL = "mic_channel"
CONF_RX_SLOT_MODE = "rx_slot_mode"
CONF_I2S_MODE = "i2s_mode"
CONF_USE_APLL = "use_apll"
CONF_I2S_NUM = "i2s_num"
CONF_MCLK_MULTIPLE = "mclk_multiple"
CONF_I2S_COMM_FMT = "i2s_comm_fmt"
CONF_RX_BUS = "rx_bus"
CONF_TX_BUS = "tx_bus"
CONF_USE_TDM_REFERENCE = "use_tdm_reference"
CONF_TDM_TOTAL_SLOTS = "tdm_total_slots"
CONF_TDM_MIC_SLOT = "tdm_mic_slot"
CONF_TDM_MIC_SLOTS = "tdm_mic_slots"
CONF_TDM_REF_SLOT = "tdm_ref_slot"
CONF_TDM_TX_SLOT = "tdm_tx_slot"
CONF_ESP_AUDIO_STACK_ID = "esp_audio_stack_id"
CONF_TASK_PRIORITY = "task_priority"
CONF_TASK_CORE = "task_core"
CONF_TASK_STACK_SIZE = "task_stack_size"
CONF_DMA_DESC_NUM = "dma_desc_num"
CONF_DMA_FRAME_NUM = "dma_frame_num"
CONF_TX_CHANNEL = "tx_channel"
CONF_SPEAKER_CHANNELS = "speaker_channels"
CONF_BUFFERS_IN_PSRAM = "buffers_in_psram"
CONF_AEC_REF_RING_IN_PSRAM = "aec_ref_ring_in_psram"
CONF_AUDIO_TASK_STACK_IN_PSRAM = "audio_task_stack_in_psram"
CONF_AEC_REFERENCE = "aec_reference"
CONF_AEC_REFERENCE_BUFFER_MS = "aec_reference_buffer_ms"
CONF_TELEMETRY = "telemetry"
CONF_TELEMETRY_LOG_INTERVAL_FRAMES = "telemetry_log_interval_frames"
CONF_AUDIO_EFFECTS = "audio_effects"
CONF_RATE_CVT_COMPLEXITY = "rate_cvt_complexity"
CONF_RATE_CVT_PERF_TYPE = "rate_cvt_perf_type"
CONF_CODEC = "codec"
CONF_MIC_SELECTED = "mic_selected"
CONF_GAIN_DB = "gain_db"
CONF_REF_CHANNEL = "ref_channel"
CONF_REF_GAIN_DB = "ref_gain_db"
CONF_USE_MCLK = "use_mclk"
CONF_NO_DAC_REF = "no_dac_ref"
CONF_ON_MIC_START = "on_mic_start"
CONF_ON_MIC_IDLE = "on_mic_idle"
CONF_ON_SPEAKER_START = "on_speaker_start"
CONF_ON_SPEAKER_IDLE = "on_speaker_idle"
CONF_ON_AMPLIFIER_REQUIRED = "on_amplifier_required"
CONF_ON_AMPLIFIER_IDLE = "on_amplifier_idle"

CODEC_KIND = {
    "es8311": 1,
    "es8388": 2,
    "es8374": 3,
    "es8389": 4,
}
RATE_CVT_PERF_TYPES = ("speed", "memory")

# Keep realtime audio builds reproducible. These component-manager versions
# are validated with the maintained ESP32-S3 and ESP32-P4 builds.
ESP_AUDIO_EFFECTS_REF = "1.3.0~1"
ESP_CODEC_DEV_REF = "1.5.10"

I2S_OPTIONAL_MCLK = cv.Any(
    cv.int_range(min=-1, max=-1),
    pins.internal_gpio_output_pin_number,
)

I2S_RX_BUS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_I2S_NUM): cv.int_range(min=0, max=2),
        cv.Required(CONF_I2S_LRCLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_I2S_BCLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_I2S_MCLK_PIN, default=-1): I2S_OPTIONAL_MCLK,
        cv.Required(CONF_I2S_DIN_PIN): pins.internal_gpio_input_pin_number,
    }
)

I2S_TX_BUS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_I2S_NUM): cv.int_range(min=0, max=2),
        cv.Required(CONF_I2S_LRCLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_I2S_BCLK_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_I2S_MCLK_PIN, default=-1): I2S_OPTIONAL_MCLK,
        cv.Required(CONF_I2S_DOUT_PIN): pins.internal_gpio_output_pin_number,
    }
)

CODEC_INPUT_SCHEMA = cv.typed_schema(
    {
        "es7210": cv.Schema(
            {
                cv.Optional(CONF_ADDRESS, default=0x40): cv.i2c_address,
                cv.Optional(CONF_MIC_SELECTED, default=0x0F): cv.hex_uint8_t,
                cv.Optional(CONF_GAIN_DB, default=30.0): cv.float_range(
                    min=0.0, max=37.5
                ),
                cv.Optional(CONF_REF_CHANNEL): cv.int_range(min=0, max=15),
                cv.Optional(CONF_REF_GAIN_DB, default=0.0): cv.float_range(
                    min=0.0, max=37.5
                ),
            }
        ),
        "es8311": cv.Schema(
            {
                cv.Optional(CONF_ADDRESS, default=0x18): cv.i2c_address,
                cv.Optional(CONF_GAIN_DB, default=30.0): cv.float_range(
                    min=0.0, max=37.5
                ),
                cv.Optional(CONF_USE_MCLK, default=True): cv.boolean,
                cv.Optional(CONF_NO_DAC_REF, default=False): cv.boolean,
            }
        ),
        "es8388": cv.Schema(
            {
                cv.Optional(CONF_ADDRESS, default=0x20): cv.i2c_address,
                cv.Optional(CONF_GAIN_DB, default=30.0): cv.float_range(
                    min=0.0, max=37.5
                ),
            }
        ),
        "es8374": cv.Schema(
            {
                cv.Optional(CONF_ADDRESS, default=0x20): cv.i2c_address,
                cv.Optional(CONF_GAIN_DB, default=30.0): cv.float_range(
                    min=0.0, max=37.5
                ),
            }
        ),
        "es8389": cv.Schema(
            {
                cv.Optional(CONF_ADDRESS, default=0x20): cv.i2c_address,
                cv.Optional(CONF_GAIN_DB, default=30.0): cv.float_range(
                    min=0.0, max=37.5
                ),
                cv.Optional(CONF_USE_MCLK, default=True): cv.boolean,
                cv.Optional(CONF_NO_DAC_REF, default=False): cv.boolean,
            }
        ),
    },
    lower=True,
    default_type="es7210",
)

CODEC_OUTPUT_SCHEMA = cv.typed_schema(
    {
        "es8311": cv.Schema(
            {
                cv.Optional(CONF_ADDRESS, default=0x18): cv.i2c_address,
                cv.Optional(CONF_USE_MCLK, default=True): cv.boolean,
                cv.Optional(CONF_NO_DAC_REF, default=True): cv.boolean,
            }
        ),
        "es8388": cv.Schema(
            {
                cv.Optional(CONF_ADDRESS, default=0x20): cv.i2c_address,
            }
        ),
        "es8374": cv.Schema(
            {
                cv.Optional(CONF_ADDRESS, default=0x20): cv.i2c_address,
            }
        ),
        "es8389": cv.Schema(
            {
                cv.Optional(CONF_ADDRESS, default=0x20): cv.i2c_address,
                cv.Optional(CONF_USE_MCLK, default=True): cv.boolean,
                cv.Optional(CONF_NO_DAC_REF, default=True): cv.boolean,
            }
        ),
    },
    lower=True,
    default_type="es8311",
)

esp_audio_stack_ns = cg.esphome_ns.namespace("esp_audio_stack")
ESPAudioStack = esp_audio_stack_ns.class_("ESPAudioStack", cg.Component)
StartAction = esp_audio_stack_ns.class_("StartAction", automation.Action)
StopAction = esp_audio_stack_ns.class_("StopAction", automation.Action)
StopAndWaitAction = esp_audio_stack_ns.class_("StopAndWaitAction", automation.Action)
IsIdleCondition = esp_audio_stack_ns.class_("IsIdleCondition", automation.Condition)

# AudioProcessor abstract interface (internal esp_audio_stack internal audio core headers)
# Both esp_aec::EspAec and esp_afe::EspAfe inherit from this adapter.
AudioProcessor = cg.esphome_ns.namespace("audio_core").class_("AudioProcessor")

# Maintained targets. All require PSRAM and ESP-IDF.
I2S_PORTS = {
    VARIANT_ESP32P4: 3,
    VARIANT_ESP32S3: 2,
}

# Supported targets with TDM support (from SOC_I2S_SUPPORTS_TDM in soc_caps.h)
TDM_VARIANTS = {
    VARIANT_ESP32S3,
    VARIANT_ESP32P4,
}


def _validate_sample_rates(config):
    """Validate that output_sample_rate divides sample_rate evenly."""
    if CONF_OUTPUT_SAMPLE_RATE in config:
        sr = config[CONF_SAMPLE_RATE]
        osr = config[CONF_OUTPUT_SAMPLE_RATE]
        if sr % osr != 0:
            raise cv.Invalid(
                f"sample_rate ({sr}) must be an exact multiple of "
                f"output_sample_rate ({osr})"
            )
        ratio = sr // osr
        if ratio > 6:
            raise cv.Invalid(
                f"Decimation ratio {ratio} (={sr}/{osr}) exceeds maximum of 6"
            )
    return config


def _validate_tdm_config(config):
    """Validate TDM reference configuration."""
    use_tdm = config.get(CONF_USE_TDM_REFERENCE, False)
    use_tdm_bus = use_tdm or CONF_TDM_MIC_SLOTS in config
    use_stereo = config.get(CONF_USE_STEREO_AEC_REFERENCE, False)

    if use_tdm and use_stereo:
        raise cv.Invalid(
            "use_tdm_reference and use_stereo_aec_reference are mutually exclusive"
        )

    if use_tdm_bus:
        if config.get(CONF_SPEAKER_CHANNELS, 1) != 1:
            raise cv.Invalid(
                "speaker_channels: 2 is only supported on STD I2S, not TDM"
            )
        total_slots = config.get(CONF_TDM_TOTAL_SLOTS, 4)
        ref_slot = config.get(CONF_TDM_REF_SLOT, 1)
        tx_slot = config.get(CONF_TDM_TX_SLOT, 0)
        mic_slots = config.get(CONF_TDM_MIC_SLOTS, [config.get(CONF_TDM_MIC_SLOT, 0)])

        if len(set(mic_slots)) != len(mic_slots):
            raise cv.Invalid("tdm_mic_slots must not contain duplicates")

        for mic_slot in mic_slots:
            if mic_slot == ref_slot:
                raise cv.Invalid(
                    f"tdm_mic_slots contains ref slot {ref_slot}; microphone and reference slots must differ"
                )

        max_slot = max([ref_slot, tx_slot, *mic_slots])
        if total_slots <= max_slot:
            raise cv.Invalid(
                f"tdm_total_slots ({total_slots}) must be > {max_slot} "
                f"(highest slot index)"
            )
    elif config.get(CONF_SPEAKER_CHANNELS, 1) > config.get(CONF_NUM_CHANNELS, 1):
        raise cv.Invalid(
            "speaker_channels: 2 requires num_channels: 2 on the physical TX bus"
        )

    return config


def _validate_pcm_format(config):
    """Validate that PCM short/long formats require TDM mode."""
    fmt = config.get(CONF_I2S_COMM_FMT, "philips")
    use_tdm_bus = (
        config.get(CONF_USE_TDM_REFERENCE, False) or CONF_TDM_MIC_SLOTS in config
    )
    if fmt in ("pcm_short", "pcm_long") and not use_tdm_bus:
        raise cv.Invalid(
            f"i2s_comm_fmt '{fmt}' requires TDM slots "
            f"(PCM short/long are TDM-only in ESP-IDF)"
        )
    return config


def _validate_dual_bus_config(config):
    """Validate optional RX/TX bus split."""
    has_rx_bus = CONF_RX_BUS in config
    has_tx_bus = CONF_TX_BUS in config
    if has_rx_bus != has_tx_bus:
        raise cv.Invalid("rx_bus and tx_bus must be configured together")

    if has_rx_bus:
        if config.get(CONF_USE_TDM_REFERENCE, False) or CONF_TDM_MIC_SLOTS in config:
            raise cv.Invalid("rx_bus/tx_bus dual-bus mode does not support TDM yet")
        if config[CONF_RX_BUS][CONF_I2S_NUM] == config[CONF_TX_BUS][CONF_I2S_NUM]:
            raise cv.Invalid("rx_bus and tx_bus must use different i2s_num values")
        for pin_key in (
            CONF_I2S_LRCLK_PIN,
            CONF_I2S_BCLK_PIN,
            CONF_I2S_DIN_PIN,
            CONF_I2S_DOUT_PIN,
        ):
            if config.get(pin_key, -1) != -1:
                raise cv.Invalid(
                    f"{pin_key} must be set inside rx_bus/tx_bus when dual-bus mode is used"
                )
    else:
        if config.get(CONF_I2S_LRCLK_PIN, -1) == -1:
            raise cv.Invalid(
                "i2s_lrclk_pin is required when rx_bus/tx_bus are not used"
            )
        if config.get(CONF_I2S_BCLK_PIN, -1) == -1:
            raise cv.Invalid("i2s_bclk_pin is required when rx_bus/tx_bus are not used")

    return config


CONFIG_SCHEMA = cv.All(
    cv.only_with_framework(Framework.ESP_IDF),
    cv.requires_component(psram.DOMAIN),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESPAudioStack),
            cv.Optional(CONF_I2S_LRCLK_PIN, default=-1): cv.Any(
                cv.int_range(min=-1, max=-1),
                pins.internal_gpio_output_pin_number,
            ),
            cv.Optional(CONF_I2S_BCLK_PIN, default=-1): cv.Any(
                cv.int_range(min=-1, max=-1),
                pins.internal_gpio_output_pin_number,
            ),
            cv.Optional(CONF_I2S_MCLK_PIN, default=-1): I2S_OPTIONAL_MCLK,
            cv.Optional(CONF_I2S_DIN_PIN, default=-1): cv.Any(
                cv.int_range(min=-1, max=-1),
                pins.internal_gpio_input_pin_number,
            ),
            cv.Optional(CONF_I2S_DOUT_PIN, default=-1): cv.Any(
                cv.int_range(min=-1, max=-1),
                pins.internal_gpio_output_pin_number,
            ),
            cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(
                min=8000, max=48000
            ),
            cv.Optional(CONF_BITS_PER_SAMPLE, default=16): cv.one_of(
                16, 24, 32, int=True
            ),
            cv.Optional(CONF_SLOT_BIT_WIDTH, default="auto"): cv.Any(
                cv.one_of("auto", lower=True),
                cv.one_of(16, 24, 32, int=True),
            ),
            cv.Optional(CONF_CORRECT_DC_OFFSET, default=False): cv.boolean,
            cv.Optional(CONF_NUM_CHANNELS, default=1): cv.one_of(1, 2, int=True),
            cv.Optional(CONF_SPEAKER_CHANNELS, default=1): cv.one_of(1, 2, int=True),
            cv.Optional(CONF_MIC_CHANNEL, default="left"): cv.one_of(
                "left", "right", lower=True
            ),
            cv.Optional(CONF_RX_SLOT_MODE, default="mono"): cv.one_of(
                "mono", "stereo", lower=True
            ),
            cv.Optional(CONF_TX_CHANNEL, default="left"): cv.one_of(
                "left", "right", lower=True
            ),
            cv.Optional(CONF_I2S_MODE, default="primary"): cv.one_of(
                "primary", "secondary", lower=True
            ),
            cv.Optional(CONF_USE_APLL, default=False): cv.boolean,
            cv.Optional(CONF_I2S_NUM, default=0): cv.int_range(min=0, max=2),
            cv.Optional(CONF_RX_BUS): I2S_RX_BUS_SCHEMA,
            cv.Optional(CONF_TX_BUS): I2S_TX_BUS_SCHEMA,
            cv.Optional(CONF_MCLK_MULTIPLE, default=256): cv.one_of(
                128, 256, 384, 512, int=True
            ),
            cv.Optional(CONF_I2S_COMM_FMT, default="philips"): cv.one_of(
                "philips",
                "msb",
                "pcm_short",
                "pcm_long",
                lower=True,
            ),
            # Output sample rate for mic/AEC/MWW/VA (converted from bus rate).
            # If omitted, equals sample_rate (no rate conversion).
            cv.Optional(CONF_OUTPUT_SAMPLE_RATE): cv.int_range(min=8000, max=48000),
            cv.Optional(CONF_PROCESSOR_ID): cv.use_id(AudioProcessor),
            # Input gain before the processor: <1.0 attenuates hot mics,
            # >1.0 amplifies weak mics. This is gain staging, not a separate mic output.
            cv.Optional(CONF_INPUT_GAIN, default=1.0): cv.float_range(
                min=0.01, max=32.0
            ),
            # 0% remains hard mute. This controls how loud mid-range percentages feel:
            # -49 dB matches ESPHome's software curve, while codec-dev defaults near
            # -50 dB if this option is omitted on hardware codec outputs.
            cv.Optional(CONF_MASTER_VOLUME_MIN_DB): cv.float_range(min=-96.0, max=0.0),
            # ES8311 digital feedback: RX is stereo with L=ADC(mic), R=DAC(reference)
            # when no_dac_ref is false. Espressif's driver writes REG44=0x58,
            # documented in-source as "ADCL + DACR".
            cv.Optional(CONF_USE_STEREO_AEC_REFERENCE, default=False): cv.boolean,
            # Which stereo channel carries the AEC reference (default: left)
            cv.Optional(CONF_REFERENCE_CHANNEL, default="left"): cv.one_of(
                "left", "right", lower=True
            ),
            # TDM hardware reference: ES7210 in TDM mode with one slot carrying DAC feedback
            cv.Optional(CONF_USE_TDM_REFERENCE, default=False): cv.boolean,
            cv.Optional(CONF_TDM_TOTAL_SLOTS, default=4): cv.int_range(min=2, max=8),
            cv.Optional(CONF_TDM_MIC_SLOT, default=0): cv.int_range(min=0, max=7),
            cv.Optional(CONF_TDM_MIC_SLOTS): cv.All(
                cv.ensure_list(cv.int_range(min=0, max=7)),
                cv.Length(min=1, max=2),
            ),
            cv.Optional(CONF_TDM_REF_SLOT, default=1): cv.int_range(min=0, max=7),
            cv.Optional(CONF_TDM_TX_SLOT, default=0): cv.int_range(min=0, max=7),
            # Audio task tuning (advanced)
            cv.Optional(CONF_TASK_PRIORITY, default=19): cv.int_range(min=1, max=24),
            cv.Optional(CONF_TASK_CORE, default=0): cv.int_range(min=-1, max=1),
            cv.Optional(CONF_TASK_STACK_SIZE, default=8192): cv.int_range(
                min=4096, max=32768
            ),
            # Official IDF i2s_chan_config_t DMA knobs. If dma_frame_num is omitted,
            # the component keeps the historical ~10 ms/descriptor auto sizing.
            cv.Optional(CONF_DMA_DESC_NUM, default=6): cv.int_range(min=2, max=16),
            cv.Optional(CONF_DMA_FRAME_NUM): cv.int_range(min=64, max=4092),
            # Use PSRAM for non-DMA audio buffers (saves ~15KB internal RAM).
            # Requires PSRAM. DMA buffers (I2S RX/TX) always use internal RAM.
            cv.Optional(CONF_BUFFERS_IN_PSRAM, default=False): cv.boolean,
            # Place the audio task's own stack in PSRAM. Saves ~8KB internal RAM
            # but increases per-frame latency because every function return has
            # to traverse PSRAM. Only enable on boards that need the internal
            # headroom (e.g. waveshare-s3 running 2-mic Speech Enhancement plus
            # concurrent TLS streams). ESPHome's PSRAM helper requests the required
            # task-stack sdkconfig when enabled.
            cv.Optional(
                CONF_AUDIO_TASK_STACK_IN_PSRAM, default=False
            ): psram.validate_task_stack_in_psram,
            # AEC reference mode for no-codec setups (ignored if stereo/TDM ref is configured):
            #   ring_buffer: Espressif ADF TYPE2-style software reference, default
            #   previous_frame: light mode using the previous TX frame, no TYPE2 ring
            cv.Optional(CONF_AEC_REFERENCE, default="ring_buffer"): cv.one_of(
                "previous_frame",
                "ring_buffer",
                lower=True,
            ),
            # Ring buffer capacity in ms (only used with aec_reference: ring_buffer)
            cv.Optional(CONF_AEC_REFERENCE_BUFFER_MS, default=80): cv.int_range(
                min=32, max=500
            ),
            # Place AEC reference ring buffer in PSRAM. Default false = internal RAM
            # (~13.6 us/frame faster on Core 0, costs ~3-5 KB internal). Set true to
            # save internal RAM at the cost of Core 0 PSRAM traffic.
            cv.Optional(CONF_AEC_REF_RING_IN_PSRAM, default=False): cv.boolean,
            # Enable per-stage cycle counting and diagnostics (debug only, adds overhead)
            cv.Optional(CONF_TELEMETRY, default=False): cv.boolean,
            cv.Optional(CONF_TELEMETRY_LOG_INTERVAL_FRAMES, default=128): cv.int_range(
                min=1, max=8192
            ),
            cv.Optional(CONF_AUDIO_EFFECTS, default={}): cv.Schema(
                {
                    cv.Optional(CONF_RATE_CVT_COMPLEXITY, default=3): cv.int_range(
                        min=1, max=3
                    ),
                    cv.Optional(CONF_RATE_CVT_PERF_TYPE, default="speed"): cv.one_of(
                        *RATE_CVT_PERF_TYPES,
                        lower=True,
                    ),
                }
            ),
            cv.Optional(CONF_CODEC): cv.Schema(
                {
                    cv.GenerateID(CONF_I2C_ID): cv.use_id(i2c.I2CBus),
                    cv.Optional(CONF_INPUT): CODEC_INPUT_SCHEMA,
                    cv.Optional(CONF_OUTPUT): CODEC_OUTPUT_SCHEMA,
                }
            ),
            # Lifecycle hooks for board-level power policy. Use the speaker hooks
            # for amp power gating; the generic audio-stack hooks also fire on mic-only
            # activity such as wake-word listeners.
            cv.Optional(CONF_ON_START): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_IDLE): automation.validate_automation(single=True),
            # Minimal runtime state machine. `state` is one of:
            # idle, mic, speaker, duplex.
            cv.Optional(CONF_ON_STATE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_MIC_START): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_MIC_IDLE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_SPEAKER_START): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_ON_SPEAKER_IDLE): automation.validate_automation(
                single=True
            ),
            # Semantic aliases for board amplifier/power-enable control. They fire
            # on the same edges as speaker playback, but keep YAML intent separate
            # from the ESPHome speaker abstraction.
            cv.Optional(CONF_ON_AMPLIFIER_REQUIRED): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_ON_AMPLIFIER_IDLE): automation.validate_automation(
                single=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_sample_rates,
    _validate_tdm_config,
    _validate_pcm_format,
    _validate_dual_bus_config,
)


def _final_validate(config):
    """Validate i2s_num against SoC port count and TDM against SoC capability."""
    variant = get_esp32_variant()
    if variant not in I2S_PORTS:
        raise cv.Invalid(
            f"esp_audio_stack requires ESP32-S3 or ESP32-P4 with PSRAM, got {variant}"
        )

    max_ports = I2S_PORTS[variant]
    if CONF_RX_BUS in config:
        if max_ports < 2:
            raise cv.Invalid(
                f"rx_bus/tx_bus dual-bus mode requires at least two I2S ports on {variant}"
            )
        for bus_key in (CONF_RX_BUS, CONF_TX_BUS):
            i2s_num = config[bus_key][CONF_I2S_NUM]
            if i2s_num >= max_ports:
                raise cv.Invalid(
                    f"{bus_key}.i2s_num {i2s_num} exceeds available I2S ports on {variant} "
                    f"(max port index: {max_ports - 1})"
                )
    else:
        i2s_num = config.get(CONF_I2S_NUM, 0)
        if i2s_num >= max_ports:
            raise cv.Invalid(
                f"i2s_num {i2s_num} exceeds available I2S ports on {variant} "
                f"(max port index: {max_ports - 1})"
            )

    use_tdm_bus = (
        config.get(CONF_USE_TDM_REFERENCE, False) or CONF_TDM_MIC_SLOTS in config
    )
    if use_tdm_bus and variant not in TDM_VARIANTS:
        raise cv.Invalid(
            f"TDM options require TDM support, but {variant} does not have SOC_I2S_SUPPORTS_TDM"
        )

    # APLL is available on ESP32-P4 in the maintained target set.
    if config.get(CONF_USE_APLL, False) and variant != VARIANT_ESP32P4:
        raise cv.Invalid(
            f"use_apll is not supported on {variant}. "
            "APLL clock source is only available on ESP32-P4 in the maintained target set."
        )

    from esphome.core import CORE

    full_config = CORE.config or {}

    # esp_aec and esp_afe are mutually exclusive: both provide AudioProcessor
    has_aec = "esp_aec" in full_config
    has_afe = "esp_afe" in full_config
    if has_aec and has_afe:
        raise cv.Invalid(
            "esp_aec and esp_afe are mutually exclusive. "
            "Use esp_afe (full AFE pipeline with AEC+NS+AGC+Speech Enhancement) "
            "or esp_aec (standalone echo cancellation), not both."
        )

    return config


FINAL_VALIDATE_SCHEMA = _final_validate


def _add_config_setters(var, config, setters):
    for key, setter in setters:
        cg.add(setter(config[key]))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    use_tdm_bus = config[CONF_USE_TDM_REFERENCE] or CONF_TDM_MIC_SLOTS in config
    use_tdm_ref = config[CONF_USE_TDM_REFERENCE]
    use_stereo_ref = config[CONF_USE_STEREO_AEC_REFERENCE]
    has_processor = CONF_PROCESSOR_ID in config
    output_rate = config.get(CONF_OUTPUT_SAMPLE_RATE, config[CONF_SAMPLE_RATE])
    use_rate_cvt = output_rate != config[CONF_SAMPLE_RATE]
    use_32bit = config[CONF_BITS_PER_SAMPLE] > 16
    use_stereo_tx = config[CONF_NUM_CHANNELS] == 2 and not use_tdm_bus
    use_rx_slot_stereo = config[CONF_RX_SLOT_MODE] == "stereo"
    use_multi_rx = use_tdm_bus or use_stereo_ref or use_rx_slot_stereo
    use_mono_rx_effects = not use_multi_rx and (use_rate_cvt or use_32bit)
    use_mono_ref = has_processor and not use_tdm_ref and not use_stereo_ref
    use_ring_ref = use_mono_ref and config[CONF_AEC_REFERENCE] == "ring_buffer"
    use_previous_frame_ref = (
        use_mono_ref and config[CONF_AEC_REFERENCE] == "previous_frame"
    )

    has_hardware_codec = CONF_CODEC in config
    has_output_codec = has_hardware_codec and CONF_OUTPUT in config[CONF_CODEC]
    add_idf_component(name="espressif/esp_audio_effects", ref=ESP_AUDIO_EFFECTS_REF)
    if has_hardware_codec:
        add_idf_component(name="espressif/esp_codec_dev", ref=ESP_CODEC_DEV_REF)
    await cg.register_component(var, config)

    # Define USE_ESP_AUDIO_STACK so other components know it's available
    cg.add_define("USE_ESP_AUDIO_STACK")
    if CONF_RX_BUS in config:
        cg.add_define("USE_ESP_AUDIO_STACK_DUAL_BUS")
    if use_tdm_bus:
        cg.add_define("USE_ESP_AUDIO_STACK_TDM_BUS")
    if use_tdm_ref:
        cg.add_define("USE_ESP_AUDIO_STACK_TDM_REF")
    if use_stereo_ref:
        cg.add_define("USE_ESP_AUDIO_STACK_STEREO_REF")
    if use_multi_rx:
        cg.add_define("USE_ESP_AUDIO_STACK_MULTI_RX")
    if use_mono_rx_effects:
        cg.add_define("USE_ESP_AUDIO_STACK_MONO_RX")
    if use_mono_ref:
        cg.add_define("USE_ESP_AUDIO_STACK_MONO_REF")
    if use_ring_ref:
        cg.add_define("USE_ESP_AUDIO_STACK_RING_REF")
    if use_previous_frame_ref:
        cg.add_define("USE_ESP_AUDIO_STACK_PREVIOUS_FRAME_REF")
    if use_stereo_tx:
        cg.add_define("USE_ESP_AUDIO_STACK_STEREO_TX")
    if use_32bit:
        cg.add_define("USE_ESP_AUDIO_STACK_32BIT")
    if has_hardware_codec:
        cg.add_define("USE_ESP_AUDIO_STACK_HARDWARE_CODEC")
        codec_conf = config[CONF_CODEC]
        codec_types = set()
        if CONF_INPUT in codec_conf:
            codec_types.add(codec_conf[CONF_INPUT][CONF_TYPE])
        if CONF_OUTPUT in codec_conf:
            codec_types.add(codec_conf[CONF_OUTPUT][CONF_TYPE])
        for codec_type in sorted(codec_types):
            cg.add_define(f"USE_ESP_AUDIO_STACK_CODEC_{codec_type.upper()}")
    if has_output_codec:
        cg.add_define("USE_ESP_AUDIO_STACK_HARDWARE_OUTPUT_CODEC")
    include_builtin_idf_component("esp_driver_i2s")
    add_idf_sdkconfig_option("CONFIG_I2S_ISR_IRAM_SAFE", True)
    if config[CONF_AUDIO_TASK_STACK_IN_PSRAM]:
        psram.request_external_task_stack()

    _add_config_setters(
        var,
        config,
        (
            (CONF_I2S_LRCLK_PIN, var.set_lrclk_pin),
            (CONF_I2S_BCLK_PIN, var.set_bclk_pin),
            (CONF_I2S_MCLK_PIN, var.set_mclk_pin),
            (CONF_I2S_DIN_PIN, var.set_din_pin),
            (CONF_I2S_DOUT_PIN, var.set_dout_pin),
            (CONF_SAMPLE_RATE, var.set_sample_rate),
            (CONF_BITS_PER_SAMPLE, var.set_bits_per_sample),
            (CONF_CORRECT_DC_OFFSET, var.set_correct_dc_offset),
            (CONF_NUM_CHANNELS, var.set_num_channels),
            (CONF_SPEAKER_CHANNELS, var.set_speaker_channels),
            (CONF_USE_APLL, var.set_use_apll),
            (CONF_I2S_NUM, var.set_i2s_num),
            (CONF_MCLK_MULTIPLE, var.set_mclk_multiple),
        ),
    )
    cg.add(var.set_i2s_mode_secondary(config[CONF_I2S_MODE] == "secondary"))

    if CONF_RX_BUS in config:
        rx_bus = config[CONF_RX_BUS]
        tx_bus = config[CONF_TX_BUS]
        cg.add(
            var.configure_rx_bus(
                rx_bus[CONF_I2S_NUM],
                rx_bus[CONF_I2S_LRCLK_PIN],
                rx_bus[CONF_I2S_BCLK_PIN],
                rx_bus[CONF_I2S_MCLK_PIN],
                rx_bus[CONF_I2S_DIN_PIN],
            )
        )
        cg.add(
            var.configure_tx_bus(
                tx_bus[CONF_I2S_NUM],
                tx_bus[CONF_I2S_LRCLK_PIN],
                tx_bus[CONF_I2S_BCLK_PIN],
                tx_bus[CONF_I2S_MCLK_PIN],
                tx_bus[CONF_I2S_DOUT_PIN],
            )
        )

    # Map comm format string to enum index
    comm_fmt_map = {"philips": 0, "msb": 1, "pcm_short": 2, "pcm_long": 3}
    cg.add(var.set_i2s_comm_fmt(comm_fmt_map[config[CONF_I2S_COMM_FMT]]))

    # Mic channel selection (for mono RX: which I2S slot to capture)
    cg.add(var.set_mic_channel_right(config[CONF_MIC_CHANNEL] == "right"))
    cg.add(var.set_rx_slot_mode_stereo(config[CONF_RX_SLOT_MODE] == "stereo"))

    # TX channel selection (for mono TX: which I2S slot to output)
    cg.add(var.set_tx_slot_right(config[CONF_TX_CHANNEL] == "right"))

    # Slot bit width: 0 = auto (match bits_per_sample)
    sbw = config[CONF_SLOT_BIT_WIDTH]
    cg.add(var.set_slot_bit_width(0 if sbw == "auto" else sbw))

    # Set output sample rate if specified (enables rate conversion).
    if CONF_OUTPUT_SAMPLE_RATE in config:
        cg.add(var.set_output_sample_rate(config[CONF_OUTPUT_SAMPLE_RATE]))

    # Set input gain before the audio processor.
    cg.add(var.set_input_gain(config[CONF_INPUT_GAIN]))
    if CONF_MASTER_VOLUME_MIN_DB in config:
        cg.add(var.set_master_volume_min_db(config[CONF_MASTER_VOLUME_MIN_DB]))

    # ES8311 digital feedback mode: stereo RX with L=mic, R=ref
    cg.add(var.set_use_stereo_aec_reference(config[CONF_USE_STEREO_AEC_REFERENCE]))

    # Reference channel selection (left or right)
    cg.add(var.set_reference_channel_right(config[CONF_REFERENCE_CHANNEL] == "right"))

    # TDM slot configuration. `use_tdm_reference` means the AEC reference comes
    # from a TDM slot; `tdm_mic_slots` alone keeps the TDM bus but lets AEC use
    # the software speaker reference path.
    if use_tdm_bus:
        cg.add(var.set_use_tdm_bus(True))
        cg.add(var.set_use_tdm_reference(config[CONF_USE_TDM_REFERENCE]))
        cg.add(var.set_tdm_total_slots(config[CONF_TDM_TOTAL_SLOTS]))
        mic_slots = config.get(CONF_TDM_MIC_SLOTS, [config[CONF_TDM_MIC_SLOT]])
        cg.add(var.set_tdm_mic_slot(mic_slots[0]))
        if len(mic_slots) > 1:
            cg.add(var.set_secondary_tdm_mic_slot(mic_slots[1]))
        cg.add(var.set_tdm_ref_slot(config[CONF_TDM_REF_SLOT]))
        cg.add(var.set_tdm_tx_slot(config[CONF_TDM_TX_SLOT]))

    if has_hardware_codec:
        codec_conf = config[CONF_CODEC]
        i2c_bus = await cg.get_variable(codec_conf[CONF_I2C_ID])
        cg.add(var.set_codec_i2c_bus(i2c_bus))
        if CONF_INPUT in codec_conf:
            input_conf = codec_conf[CONF_INPUT]
            if input_conf[CONF_TYPE] == "es7210":
                has_ref_gain = CONF_REF_CHANNEL in input_conf
                cg.add(
                    var.configure_es7210_codec(
                        input_conf[CONF_ADDRESS],
                        input_conf[CONF_MIC_SELECTED],
                        input_conf[CONF_GAIN_DB],
                        has_ref_gain,
                        input_conf.get(CONF_REF_CHANNEL, 0),
                        input_conf[CONF_REF_GAIN_DB],
                    )
                )
            elif input_conf[CONF_TYPE] == "es8311":
                cg.add(
                    var.configure_es8311_input_codec(
                        input_conf[CONF_ADDRESS],
                        input_conf.get(CONF_USE_MCLK, True),
                        input_conf.get(CONF_NO_DAC_REF, False),
                        input_conf[CONF_GAIN_DB],
                    )
                )
            else:
                cg.add(
                    var.configure_input_codec(
                        CODEC_KIND[input_conf[CONF_TYPE]],
                        input_conf[CONF_ADDRESS],
                        input_conf.get(CONF_USE_MCLK, True),
                        input_conf.get(CONF_NO_DAC_REF, False),
                        input_conf[CONF_GAIN_DB],
                    )
                )
        if CONF_OUTPUT in codec_conf:
            output_conf = codec_conf[CONF_OUTPUT]
            if output_conf[CONF_TYPE] == "es8311":
                cg.add(
                    var.configure_es8311_codec(
                        output_conf[CONF_ADDRESS],
                        output_conf.get(CONF_USE_MCLK, True),
                        output_conf.get(CONF_NO_DAC_REF, True),
                    )
                )
            else:
                cg.add(
                    var.configure_output_codec(
                        CODEC_KIND[output_conf[CONF_TYPE]],
                        output_conf[CONF_ADDRESS],
                        output_conf.get(CONF_USE_MCLK, True),
                        output_conf.get(CONF_NO_DAC_REF, True),
                    )
                )

    # Audio task tuning
    _add_config_setters(
        var,
        config,
        (
            (CONF_TASK_PRIORITY, var.set_task_priority),
            (CONF_TASK_CORE, var.set_task_core),
            (CONF_TASK_STACK_SIZE, var.set_task_stack_size),
            (CONF_DMA_DESC_NUM, var.set_dma_desc_num),
            (CONF_BUFFERS_IN_PSRAM, var.set_buffers_in_psram),
            (CONF_AUDIO_TASK_STACK_IN_PSRAM, var.set_audio_task_stack_in_psram),
        ),
    )
    if CONF_DMA_FRAME_NUM in config:
        cg.add(var.set_dma_frame_num(config[CONF_DMA_FRAME_NUM]))
    audio_effects = config[CONF_AUDIO_EFFECTS]
    cg.add(var.set_rate_cvt_complexity(audio_effects[CONF_RATE_CVT_COMPLEXITY]))
    cg.add(
        var.set_rate_cvt_perf_type(
            0 if audio_effects[CONF_RATE_CVT_PERF_TYPE] == "memory" else 1
        )
    )

    # AEC reference mode is compile-time selected for no-codec setups:
    # ring_buffer pulls in the Espressif/ADF TYPE2-style software reference,
    # previous_frame keeps that path out of the build.
    if use_ring_ref:
        _add_config_setters(
            var,
            config,
            (
                (CONF_AEC_REFERENCE_BUFFER_MS, var.set_aec_ref_buffer_ms),
                (CONF_AEC_REF_RING_IN_PSRAM, var.set_aec_ref_ring_in_psram),
            ),
        )

    # Telemetry: per-stage cycle counting and diagnostics
    if config[CONF_TELEMETRY]:
        cg.add_define("USE_ESP_AUDIO_STACK_TELEMETRY")
        cg.add(
            var.set_telemetry_log_interval_frames(
                config[CONF_TELEMETRY_LOG_INTERVAL_FRAMES]
            )
        )

    for key, trigger_getter, args in (
        (CONF_ON_START, var.get_start_trigger, []),
        (CONF_ON_IDLE, var.get_idle_trigger, []),
        (CONF_ON_STATE, var.get_state_trigger, [(cg.std_string, "state")]),
        (CONF_ON_MIC_START, var.get_mic_start_trigger, []),
        (CONF_ON_MIC_IDLE, var.get_mic_idle_trigger, []),
        (CONF_ON_SPEAKER_START, var.get_speaker_start_trigger, []),
        (CONF_ON_SPEAKER_IDLE, var.get_speaker_idle_trigger, []),
        (CONF_ON_AMPLIFIER_REQUIRED, var.get_speaker_start_trigger, []),
        (CONF_ON_AMPLIFIER_IDLE, var.get_speaker_idle_trigger, []),
    ):
        if key in config:
            await automation.build_automation(trigger_getter(), args, config[key])

    # Link audio processor if configured
    if CONF_PROCESSOR_ID in config:
        processor = await cg.get_variable(config[CONF_PROCESSOR_ID])
        cg.add(var.set_processor(processor))
        cg.add_define("USE_AUDIO_PROCESSOR")


# === Actions ===
ESP_AUDIO_STACK_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(ESPAudioStack)}
)


@automation.register_action(
    "esp_audio_stack.start",
    StartAction,
    ESP_AUDIO_STACK_ACTION_SCHEMA,
    synchronous=True,
)
@automation.register_action(
    "esp_audio_stack.stop", StopAction, ESP_AUDIO_STACK_ACTION_SCHEMA, synchronous=True
)
@automation.register_action(
    "esp_audio_stack.stop_and_wait",
    StopAndWaitAction,
    ESP_AUDIO_STACK_ACTION_SCHEMA,
    synchronous=True,
)
async def esp_audio_stack_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_condition(
    "esp_audio_stack.is_idle", IsIdleCondition, ESP_AUDIO_STACK_ACTION_SCHEMA
)
async def esp_audio_stack_is_idle_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
