"""Validate the hardware and audio format required by the I2S AEC adapter."""

from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.core import CORE

BASE_CONFIG = """
esphome:
  name: aec-test
esp32:
  variant: esp32
  framework:
    type: esp-idf
psram:
  mode: quad
i2s_audio:
  id: audio_bus
  i2s_bclk_pin: GPIO5
  i2s_lrclk_pin: GPIO25
  full_duplex: true
  keep_tx_active: true
microphone:
  - platform: i2s_audio
    id: raw_microphone
    i2s_audio_id: audio_bus
    adc_type: external
    i2s_din_pin: GPIO35
    sample_rate: 48000
    bits_per_sample: 16bit
    channel: stereo
  - platform: resampler
    id: voice_microphone
    microphone:
      microphone: raw_microphone
      channels: 0
    sample_rate: 16000
speaker:
  - platform: i2s_audio
    id: audio_speaker
    i2s_audio_id: audio_bus
    dac_type: external
    i2s_dout_pin: GPIO26
    sample_rate: 48000
    bits_per_sample: 16bit
    channel: stereo
i2s_aec_microphone:
  id: echo_cancelled_microphone
  i2s_audio_id: audio_bus
  microphone: voice_microphone
"""

RAW_MICROPHONE_CONFIG = (
    BASE_CONFIG.replace(
        "  - platform: resampler\n    id: voice_microphone\n    microphone:\n      microphone: raw_microphone\n      channels: 0\n    sample_rate: 16000\n",
        "",
    )
    .replace("  microphone: voice_microphone", "  microphone: raw_microphone")
    .replace("    channel: stereo\nspeaker:", "    channel: left\nspeaker:")
)
DIRECT_16KHZ_CONFIG = RAW_MICROPHONE_CONFIG.replace(
    "    sample_rate: 48000", "    sample_rate: 16000"
)


@pytest.mark.parametrize(
    ("configuration", "expected_error"),
    [
        (BASE_CONFIG, None),
        (DIRECT_16KHZ_CONFIG, None),
        (RAW_MICROPHONE_CONFIG, "16000"),
        (BASE_CONFIG.replace("psram:\n  mode: quad\n", ""), "requires PSRAM"),
        (
            BASE_CONFIG.replace("  full_duplex: true\n", "  full_duplex: false\n"),
            "requires 'full_duplex'",
        ),
        (
            BASE_CONFIG.replace(
                "  keep_tx_active: true\n", "  keep_tx_active: false\n"
            ),
            "keep_tx_active enabled",
        ),
        (
            BASE_CONFIG.replace("  keep_tx_active: true\n", ""),
            "keep_tx_active enabled",
        ),
        (
            BASE_CONFIG.replace(
                "    channel: stereo\ni2s_aec_microphone:",
                "    channel: mono\ni2s_aec_microphone:",
            ),
            "16 or 48 kHz, 16-bit stereo I2S output",
        ),
        (
            BASE_CONFIG.replace("  variant: esp32\n", "  variant: esp32s3\n")
            .replace("GPIO25", "GPIO15")
            .replace("GPIO35", "GPIO18")
            .replace("GPIO26", "GPIO16"),
            "only available on ESP32",
        ),
    ],
)
def test_aec_configuration(
    configuration: str, expected_error: str | None, tmp_path: Path
) -> None:
    path = tmp_path / "aec.yaml"
    path.write_text(configuration, encoding="utf-8")
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    errors = [str(error) for error in result.errors]
    if expected_error is None:
        assert errors == []
    else:
        assert any(expected_error in error for error in errors), errors
