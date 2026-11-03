"""Test playback timeout C++ generation."""

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.mark.parametrize("timeout", [None, "2s", "2000ms", "1ms", "10s"])
def test_playback_timeout_codegen(
    timeout: str | None,
    tmp_path: Path,
    generate_main: Callable[[Path], str],
) -> None:
    config = tmp_path / "voice_assistant.yaml"
    config.write_text(
        """esphome:
  name: test-voice-assistant
esp32:
  board: esp32dev
  framework:
    type: esp-idf
wifi:
  ssid: test-network
api:
i2s_audio:
  i2s_lrclk_pin: GPIO25
  i2s_bclk_pin: GPIO26
microphone:
  - platform: i2s_audio
    id: test_mic
    adc_type: external
    i2s_din_pin: GPIO34
voice_assistant:
  microphone: test_mic
"""
        + (f"  tts_playback_start_timeout: {timeout}\n" if timeout else "")
    )
    main = generate_main(config)
    if timeout in (None, "2s", "2000ms"):
        assert "set_tts_playback_start_timeout(" not in main
    else:
        milliseconds = 1 if timeout == "1ms" else 10000
        assert f"set_tts_playback_start_timeout({milliseconds});" in main
