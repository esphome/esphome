from esphome import automation
import esphome.codegen as cg
from esphome.components import psram
from esphome.components.esp32 import add_idf_component
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_SAMPLE_RATE, Framework
from esphome.core import CORE

CODEOWNERS = ["@n-IA-hane"]
DEPENDENCIES = ["esp32"]

# Only available on ESP32-S3 or ESP32-P4 with ESP-SR
_AEC_SUPPORTED_VARIANTS = ("ESP32S3", "ESP32P4")


def _validate_esp32_variant(config):
    if CORE.is_esp32:
        from esphome.components import esp32

        variant = esp32.get_esp32_variant()
        if variant not in _AEC_SUPPORTED_VARIANTS:
            raise cv.Invalid(
                f"esp_aec requires {' or '.join(_AEC_SUPPORTED_VARIANTS)}, got {variant}"
            )
    return config


AudioProcessor = cg.esphome_ns.namespace("audio_core").class_("AudioProcessor")

esp_aec_ns = cg.esphome_ns.namespace("esp_aec")
EspAec = esp_aec_ns.class_("EspAec", cg.Component, AudioProcessor)
SetModeAction = esp_aec_ns.class_("SetModeAction", automation.Action)

CONF_FILTER_LENGTH = "filter_length"

AEC_MODES = {
    "sr_low_cost": 0,  # AEC_MODE_SR_LOW_COST
    "sr_high_perf": 1,  # AEC_MODE_SR_HIGH_PERF
    "voip_low_cost": 3,  # AEC_MODE_VOIP_LOW_COST
    "voip_high_perf": 4,  # AEC_MODE_VOIP_HIGH_PERF
    "fd_low_cost": 5,  # AEC_MODE_FD_LOW_COST  (esp-sr 2.4+, full-duplex with NLP)
    "fd_high_perf": 6,  # AEC_MODE_FD_HIGH_PERF (esp-sr 2.4+, full-duplex with NLP)
}

CONFIG_SCHEMA = cv.All(
    cv.only_with_framework(Framework.ESP_IDF),
    cv.requires_component(psram.DOMAIN),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(EspAec),
            cv.Optional(CONF_SAMPLE_RATE, default=16000): cv.int_range(
                min=16000, max=16000
            ),
            cv.Optional(CONF_FILTER_LENGTH, default=4): cv.int_range(min=1, max=8),
            cv.Optional(CONF_MODE, default="sr_low_cost"): cv.enum(
                AEC_MODES, lower=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_esp32_variant,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_sample_rate(config[CONF_SAMPLE_RATE]))
    cg.add(var.set_filter_length(config[CONF_FILTER_LENGTH]))
    cg.add(var.set_mode(config[CONF_MODE]))

    cg.add_define("USE_AUDIO_PROCESSOR")

    # esp-sr 2.4.x requires esp-dsp >=1.8.0. Declare the lower bound here so
    # other components can still resolve the newest compatible esp-dsp release.
    add_idf_component(name="espressif/esp-dsp", ref="^1.8.0")
    # esp_aec wraps the ESP-SR v2 low-level afe_aec API directly so no-codec
    # devices do not need the full GMF pipeline or codec stack. Track the
    # current 2.4.x line for Espressif's full-duplex AEC fixes.
    add_idf_component(name="espressif/esp-sr", ref="^2.4.6")


@automation.register_action(
    "esp_aec.set_mode",
    SetModeAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(EspAec),
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
