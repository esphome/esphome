from pathlib import Path

from esphome import automation
import esphome.codegen as cg
from esphome.components import psram
from esphome.components.esp32 import add_idf_component
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_TYPE, Framework
from esphome.core import CORE

CODEOWNERS = ["@n-IA-hane"]
DEPENDENCIES = ["esp32"]

_SUPPORTED_VARIANTS = ("ESP32S3", "ESP32P4")
GMF_AI_AUDIO_PATH = Path(__file__).parent / "idf_components" / "gmf_ai_audio"


def _validate_esp32_variant(config):
    if CORE.is_esp32:
        from esphome.components import esp32

        variant = esp32.get_esp32_variant()
        if variant not in _SUPPORTED_VARIANTS:
            raise cv.Invalid(
                f"esp_afe requires {' or '.join(_SUPPORTED_VARIANTS)}, got {variant}"
            )
    return config


def _validate_feature_config(config):
    if config[CONF_SE_ENABLED] and config[CONF_MIC_NUM] < 2:
        raise cv.Invalid("se_enabled requires mic_num: 2")
    if config[CONF_MIC_NUM] >= 2 and not config[CONF_SE_ENABLED]:
        raise cv.Invalid(
            "dual-mic esp_afe requires se_enabled: true (SE/BSS is structural)"
        )
    if config.get(CONF_INPUT_FORMAT) in ("MMR", "MMNR") and config[CONF_MIC_NUM] < 2:
        raise cv.Invalid(
            f"input_format: {config[CONF_INPUT_FORMAT]} requires mic_num: 2"
        )
    return config


AudioProcessor = cg.esphome_ns.namespace("audio_core").class_("AudioProcessor")

esp_afe_ns = cg.esphome_ns.namespace("esp_afe")
EspAfe = esp_afe_ns.class_("EspAfe", cg.Component, AudioProcessor)
SetModeAction = esp_afe_ns.class_("SetModeAction", automation.Action)

CONF_ESP_AFE_ID = "esp_afe_id"
CONF_MIC_NUM = "mic_num"
CONF_AEC_ENABLED = "aec_enabled"
CONF_AEC_FILTER_LENGTH = "aec_filter_length"
CONF_AEC_NLP_LEVEL = "aec_nlp_level"
CONF_SE_ENABLED = "se_enabled"
CONF_NS_ENABLED = "ns_enabled"
CONF_VAD_ENABLED = "vad_enabled"
CONF_AGC_ENABLED = "agc_enabled"
CONF_AGC_COMPRESSION_GAIN = "agc_compression_gain"
CONF_AGC_TARGET_LEVEL = "agc_target_level"
CONF_VAD_MODE = "vad_mode"
CONF_VAD_MIN_SPEECH_MS = "vad_min_speech_ms"
CONF_VAD_MIN_NOISE_MS = "vad_min_noise_ms"
CONF_VAD_DELAY_MS = "vad_delay_ms"
CONF_VAD_MUTE_PLAYBACK = "vad_mute_playback"
CONF_VAD_ENABLE_CHANNEL_TRIGGER = "vad_enable_channel_trigger"
CONF_CONTINUOUS_VAD = "continuous_vad"
CONF_MEMORY_ALLOC_MODE = "memory_alloc_mode"
CONF_AFE_LINEAR_GAIN = "afe_linear_gain"
CONF_TASK_CORE = "task_core"
CONF_TASK_PRIORITY = "task_priority"
CONF_RINGBUF_SIZE = "ringbuf_size"
CONF_FEED_TASK_CORE = "feed_task_core"
CONF_FEED_TASK_PRIORITY = "feed_task_priority"
CONF_FEED_TASK_STACK_SIZE = "feed_task_stack_size"
CONF_FETCH_TASK_CORE = "fetch_task_core"
CONF_FETCH_TASK_PRIORITY = "fetch_task_priority"
CONF_FETCH_TASK_STACK_SIZE = "fetch_task_stack_size"
CONF_FEED_BUF_IN_PSRAM = "feed_buf_in_psram"
CONF_FEED_RING_IN_PSRAM = "feed_ring_in_psram"
CONF_FETCH_RING_IN_PSRAM = "fetch_ring_in_psram"
CONF_INPUT_FORMAT = "input_format"

AFE_TYPES = {
    "sr": 0,  # AFE_TYPE_SR: speech recognition, linear AEC (preserves spectrum for MWW)
    "vc": 1,  # AFE_TYPE_VC: voice communication, nonlinear AEC (residual suppressor)
    "fd": 3,  # AFE_TYPE_FD: full-audio stack pipeline (esp-sr 2.4+), NLP baked in for two-way speech
}

AFE_MODES = {
    "low_cost": 0,  # AFE_MODE_LOW_COST
    "high_perf": 1,  # AFE_MODE_HIGH_PERF
}

AEC_NLP_LEVELS = {
    "normal": 0,
    "aggressive": 1,
    "very_aggressive": 2,
}

MEMORY_ALLOC_MODES = {
    "more_internal": 1,
    "internal_psram_balance": 2,
    "more_psram": 3,
}

INPUT_FORMATS = {
    "auto": "",
    "mr": "MR",
    "mnr": "MNR",
    "mmr": "MMR",
    "mmnr": "MMNR",
}


def _validate_task_layout(config):
    if config[CONF_FEED_TASK_CORE] == config[CONF_FETCH_TASK_CORE]:
        raise cv.Invalid(
            "feed_task_core and fetch_task_core must be different; "
            "Espressif GMF AFE troubleshooting recommends separate cores "
            "to avoid AFE task watchdog timeouts"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.only_with_framework(Framework.ESP_IDF),
    cv.requires_component(psram.DOMAIN),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EspAfe),
            cv.Optional(CONF_TYPE, default="sr"): cv.enum(AFE_TYPES, lower=True),
            cv.Optional(CONF_MODE, default="low_cost"): cv.enum(AFE_MODES, lower=True),
            cv.Optional(CONF_MIC_NUM, default=1): cv.int_range(min=1, max=2),
            cv.Optional(CONF_AEC_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_AEC_FILTER_LENGTH, default=4): cv.int_range(min=1, max=8),
            cv.Optional(CONF_AEC_NLP_LEVEL, default="aggressive"): cv.enum(
                AEC_NLP_LEVELS, lower=True
            ),
            cv.Optional(CONF_SE_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_NS_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_VAD_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_VAD_MODE, default=3): cv.int_range(min=0, max=4),
            cv.Optional(CONF_VAD_MIN_SPEECH_MS, default=128): cv.int_range(
                min=32, max=60000
            ),
            cv.Optional(CONF_VAD_MIN_NOISE_MS, default=1000): cv.int_range(
                min=64, max=60000
            ),
            cv.Optional(CONF_VAD_DELAY_MS, default=128): cv.int_range(min=0, max=60000),
            cv.Optional(CONF_VAD_MUTE_PLAYBACK, default=False): cv.boolean,
            cv.Optional(CONF_VAD_ENABLE_CHANNEL_TRIGGER, default=False): cv.boolean,
            # When true, VAD is allowed to keep the microphone path alive
            # without an external consumer. Default false keeps VAD available as
            # a runtime feature, but prevents boot/background mic ownership.
            cv.Optional(CONF_CONTINUOUS_VAD, default=False): cv.boolean,
            cv.Optional(CONF_AGC_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_AGC_COMPRESSION_GAIN, default=9): cv.int_range(
                min=0, max=30
            ),
            cv.Optional(CONF_AGC_TARGET_LEVEL, default=3): cv.int_range(min=0, max=31),
            cv.Optional(CONF_MEMORY_ALLOC_MODE, default="more_psram"): cv.enum(
                MEMORY_ALLOC_MODES, lower=True
            ),
            cv.Optional(CONF_AFE_LINEAR_GAIN, default=1.0): cv.float_range(
                min=0.1, max=10.0
            ),
            cv.Optional(CONF_TASK_CORE, default=1): cv.int_range(min=0, max=1),
            cv.Optional(CONF_TASK_PRIORITY, default=5): cv.int_range(min=1, max=24),
            cv.Optional(CONF_RINGBUF_SIZE, default=8): cv.int_range(min=2, max=32),
            # Official esp_gmf_afe_manager task knobs. Espressif defaults put
            # feed and fetch on different cores to avoid AFE task watchdog
            # contention under load.
            cv.Optional(CONF_FEED_TASK_CORE, default=0): cv.int_range(min=0, max=1),
            cv.Optional(CONF_FEED_TASK_PRIORITY, default=5): cv.int_range(
                min=1, max=23
            ),
            cv.Optional(CONF_FEED_TASK_STACK_SIZE, default=3072): cv.int_range(
                min=1024, max=16384
            ),
            cv.Optional(CONF_FETCH_TASK_CORE, default=1): cv.int_range(min=0, max=1),
            cv.Optional(CONF_FETCH_TASK_PRIORITY, default=5): cv.int_range(
                min=1, max=23
            ),
            cv.Optional(CONF_FETCH_TASK_STACK_SIZE, default=3072): cv.int_range(
                min=1024, max=16384
            ),
            # Optional esp-sr AFE input format override for diagnostics and
            # board-specific dual-mic layouts. Default auto preserves the
            # historical MR/MMR behavior. On single-mic runtime shape the
            # override is ignored and MR is used.
            cv.Optional(CONF_INPUT_FORMAT, default="auto"): cv.enum(
                INPUT_FORMATS, lower=True
            ),
            # Buffer placement knobs. Default false = internal RAM (fastest on
            # Core 0); set true to free internal at the cost of PSRAM traffic.
            #   feed_buf_in_psram   : direct/split-frame scratch only; 1:1 GMF feeds write into the ring slot
            #   feed_ring_in_psram  : ~12 KB staging ring written every frame (~20 us/frame)
            #   fetch_ring_in_psram : ~4 KB output ring read every frame (~6.8 us/frame)
            cv.Optional(CONF_FEED_BUF_IN_PSRAM, default=False): cv.boolean,
            cv.Optional(CONF_FEED_RING_IN_PSRAM, default=False): cv.boolean,
            cv.Optional(CONF_FETCH_RING_IN_PSRAM, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_esp32_variant,
    _validate_feature_config,
    _validate_task_layout,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_afe_type(config[CONF_TYPE]))
    cg.add(var.set_afe_mode(config[CONF_MODE]))
    cg.add(var.set_mic_num(config[CONF_MIC_NUM]))
    cg.add(var.set_aec_enabled(config[CONF_AEC_ENABLED]))
    cg.add(var.set_aec_filter_length(config[CONF_AEC_FILTER_LENGTH]))
    cg.add(var.set_aec_nlp_level(config[CONF_AEC_NLP_LEVEL]))
    cg.add(var.set_se_enabled(config[CONF_SE_ENABLED]))
    cg.add(var.set_ns_enabled(config[CONF_NS_ENABLED]))
    cg.add(var.set_vad_enabled(config[CONF_VAD_ENABLED]))
    cg.add(var.set_vad_mode(config[CONF_VAD_MODE]))
    cg.add(var.set_vad_min_speech_ms(config[CONF_VAD_MIN_SPEECH_MS]))
    cg.add(var.set_vad_min_noise_ms(config[CONF_VAD_MIN_NOISE_MS]))
    cg.add(var.set_vad_delay_ms(config[CONF_VAD_DELAY_MS]))
    cg.add(var.set_vad_mute_playback(config[CONF_VAD_MUTE_PLAYBACK]))
    cg.add(var.set_vad_enable_channel_trigger(config[CONF_VAD_ENABLE_CHANNEL_TRIGGER]))
    cg.add(var.set_continuous_vad(config[CONF_CONTINUOUS_VAD]))
    cg.add(var.set_agc_enabled(config[CONF_AGC_ENABLED]))
    cg.add(var.set_agc_compression_gain(config[CONF_AGC_COMPRESSION_GAIN]))
    cg.add(var.set_agc_target_level(config[CONF_AGC_TARGET_LEVEL]))
    cg.add(var.set_memory_alloc_mode(config[CONF_MEMORY_ALLOC_MODE]))
    cg.add(var.set_afe_linear_gain(config[CONF_AFE_LINEAR_GAIN]))
    cg.add(var.set_task_core(config[CONF_TASK_CORE]))
    cg.add(var.set_task_priority(config[CONF_TASK_PRIORITY]))
    cg.add(var.set_ringbuf_size(config[CONF_RINGBUF_SIZE]))
    cg.add(var.set_feed_task_core(config[CONF_FEED_TASK_CORE]))
    cg.add(var.set_feed_task_priority(config[CONF_FEED_TASK_PRIORITY]))
    cg.add(var.set_feed_task_stack_size(config[CONF_FEED_TASK_STACK_SIZE]))
    cg.add(var.set_fetch_task_core(config[CONF_FETCH_TASK_CORE]))
    cg.add(var.set_fetch_task_priority(config[CONF_FETCH_TASK_PRIORITY]))
    cg.add(var.set_fetch_task_stack_size(config[CONF_FETCH_TASK_STACK_SIZE]))
    cg.add(var.set_input_format_override(config[CONF_INPUT_FORMAT]))
    cg.add(var.set_feed_buf_in_psram(config[CONF_FEED_BUF_IN_PSRAM]))
    cg.add(var.set_feed_ring_in_psram(config[CONF_FEED_RING_IN_PSRAM]))
    cg.add(var.set_fetch_ring_in_psram(config[CONF_FETCH_RING_IN_PSRAM]))

    cg.add_define("USE_AUDIO_PROCESSOR")

    # esp-sr 2.4.x requires esp-dsp >=1.8.0. Declare the lower bound here so
    # downstream audio consumers do not inherit an older transitive esp-dsp pin.
    add_idf_component(name="espressif/esp-dsp", ref="^1.8.0")

    if config[CONF_MIC_NUM] <= 1:
        cg.add_define("USE_ESP_AFE_DIRECT_PATH")
        add_idf_component(name="espressif/esp-sr", ref="^2.4.6")
    if config[CONF_MIC_NUM] >= 2:
        cg.add_define("USE_ESP_AFE_GMF_PATH")
        # Local fork of Espressif GMF AI Audio 1.0.0~1. Upstream pins esp-sr
        # 2.4.4 even though the ESP-SR headers and public symbols used by GMF
        # are compatible with 2.4.6 on S3 and P4. Keep the fork minimal: only
        # the manifest dependency is bumped so dual-mic AFE can pick up
        # Espressif's 2.4.5/2.4.6 SR fixes without changing GMF source logic.
        add_idf_component(
            name="espressif/gmf_ai_audio",
            path=str(GMF_AI_AUDIO_PATH),
        )


@automation.register_action(
    "esp_afe.set_mode",
    SetModeAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EspAfe),
            cv.Required(CONF_MODE): cv.templatable(cv.string),
        }
    ),
    synchronous=True,
)
async def set_mode_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    parent = await cg.get_variable(config[CONF_ID])
    cg.add(var.set_parent(parent))
    templ = await cg.templatable(config[CONF_MODE], args, cg.std_string)
    cg.add(var.set_mode(templ))
    return var
