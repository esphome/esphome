"""A2DP speaker output for a nearby Bluetooth audio sink."""

import esphome.codegen as cg
from esphome.components import audio, esp32, speaker
from esphome.components.const import CONF_SLOT
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@mbrudka"]
DEPENDENCIES = ["esp32_ble"]
AUTO_LOAD = ["a2dp_source", "ring_buffer"]

a2dp_source_ns = cg.esphome_ns.namespace("a2dp_source")
A2DPSourceSpeaker = a2dp_source_ns.class_(
    "A2DPSourceSpeaker", cg.Component, speaker.Speaker
)


def _set_stream_limits(config: ConfigType) -> ConfigType:
    audio.set_stream_limits(
        min_bits_per_sample=16,
        max_bits_per_sample=16,
        min_channels=2,
        max_channels=2,
        min_sample_rate=44100,
        max_sample_rate=44100,
    )(config)
    return config


CONFIG_SCHEMA = cv.All(
    speaker.SPEAKER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(A2DPSourceSpeaker),
            cv.Optional(CONF_ADDRESS): cv.mac_address,
            cv.Optional(CONF_SLOT, default=1): cv.int_range(min=1, max=8),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _set_stream_limits,
)


def final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    if esp32.get_esp32_variant() != esp32.VARIANT_ESP32:
        raise cv.Invalid("A2DP source requires the original ESP32")
    if not full_config["esp32_ble"]["classic_bt"]:
        raise cv.Invalid("A2DP source requires esp32_ble.classic_bt: true")
    speakers = [
        item
        for item in full_config.get("speaker", [])
        if item.get("platform") == "a2dp_source"
    ]
    if len({item[CONF_SLOT] for item in speakers}) != len(speakers):
        raise cv.Invalid("A2DP source slot numbers must be unique")
    addresses = [str(item[CONF_ADDRESS]) for item in speakers if CONF_ADDRESS in item]
    if len(set(addresses)) != len(addresses):
        raise cv.Invalid("A2DP source addresses must be unique")


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await speaker.register_speaker(var, config)
    cg.add(var.set_slot(config[CONF_SLOT]))
    if CONF_ADDRESS in config:
        cg.add(var.set_address(str(config[CONF_ADDRESS])))
    esp32.add_idf_sdkconfig_option("CONFIG_BT_A2DP_ENABLE", True)
    esp32.add_idf_sdkconfig_option("CONFIG_BT_A2DP_USE_EXTERNAL_CODEC", False)
    cg.add_define("USE_A2DP_SOURCE")
