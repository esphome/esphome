"""Transcoder component for managing hardware media codecs (JPEG, H.264, etc.)."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

CODEOWNERS = ["@p1ngb4ck"]
DEPENDENCIES = []

transcoder_ns = cg.esphome_ns.namespace("transcoder")
Transcoder = transcoder_ns.class_("Transcoder", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Transcoder),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Configure transcoder component based on platform."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Only configure for ESP32 platforms
    if not CORE.is_esp32:
        return

    from esphome.components.esp32 import get_esp32_variant, add_idf_component

    variant = get_esp32_variant()
    if not variant:
        return

    variant_lower = variant.lower().replace("-", "")

    # JPEG Decoder/Encoder Support
    if variant_lower in ("esp32s2", "esp32s2"):
        # ESP32-S2/S3: Use esp_jpeg from ESP Component Registry
        add_idf_component(name="espressif/esp_jpeg", ref="1.3.1")
        cg.add_define("USE_ESP_JPEG_DECODER")
        cg.add_define("USE_ESP_JPEG_ENCODER")
        cg.add_define("TRANSCODER_JPEG_AVAILABLE")
    elif variant_lower in ("esp32s3", "esp32s3"):
        # ESP32-S2/S3: Use esp_jpeg from ESP Component Registry
        add_idf_component(name="espressif/esp_jpeg", ref="1.3.1")
        cg.add_define("USE_ESP_JPEG_DECODER")
        cg.add_define("USE_ESP_JPEG_ENCODER")
        cg.add_define("TRANSCODER_JPEG_AVAILABLE")
    elif variant_lower in ("esp32p4", "esp32p4"):
        # ESP32-P4: Hardware JPEG codec
        cg.add_define("USE_HARDWARE_JPEG_DECODER")
        cg.add_define("USE_HARDWARE_JPEG_ENCODER")
        cg.add_define("TRANSCODER_JPEG_AVAILABLE")

        # H.264 Decoder/Encoder Support (ESP32-P4 only)
        # Note: H.264 support requires ESP-IDF 5.3+ and is hardware-accelerated on P4
        cg.add_define("USE_HARDWARE_H264_DECODER")
        cg.add_define("USE_HARDWARE_H264_ENCODER")
        cg.add_define("TRANSCODER_H264_AVAILABLE")

    # Set global transcoder accessor
    cg.add_define("USE_TRANSCODER")
    cg.add(cg.RawExpression("esphome::transcoder::global_transcoder = id(transcoder_instance)"))
