"""A2DP speaker output for a nearby Bluetooth audio sink."""

import esphome.codegen as cg
from esphome.components import audio, esp32, esp32_ble, speaker
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
            cv.Required(CONF_ADDRESS): cv.mac_address,
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
    if (
        sum(
            item.get("platform") == "a2dp_source"
            for item in full_config.get("speaker", [])
        )
        != 1
    ):
        raise cv.Invalid("This A2DP source version supports one receiver")
    esp32_ble.register_bt_logger(
        esp32_ble.BTLoggers.A2D,
        esp32_ble.BTLoggers.AVDT,
        esp32_ble.BTLoggers.AVCT,
        esp32_ble.BTLoggers.AVRC,
        esp32_ble.BTLoggers.SDP,
    )


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await speaker.register_speaker(var, config)
    cg.add(var.set_address(str(config[CONF_ADDRESS])))
    esp32.add_idf_sdkconfig_option("CONFIG_BT_A2DP_ENABLE", True)
    esp32.add_idf_sdkconfig_option("CONFIG_BT_A2DP_USE_EXTERNAL_CODEC", False)
    cg.add_define("USE_A2DP_SOURCE")
