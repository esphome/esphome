"""Host-side checks for the fixed-ratio microphone decimator."""

from pathlib import Path
import shutil
import subprocess

import pytest

from esphome import config, yaml_util
from esphome.core import CORE


@pytest.mark.parametrize(
    ("source_rate", "target_rate", "channels", "expected_error"),
    [
        (48000, 16000, "0", None),
        (48000, 32000, "0", "requires 16 kHz output"),
        (48000, 16000, "[0, 1]", "requires 16 kHz output"),
        (32000, 16000, "0", "requires a 48 kHz source"),
    ],
)
def test_fast_decimation_yaml_validation(
    source_rate: int,
    target_rate: int,
    channels: str,
    expected_error: str | None,
    tmp_path: Path,
) -> None:
    path = tmp_path / "decimation.yaml"
    path.write_text(
        f"""
esphome:
  name: decimation-test
esp32:
  variant: esp32
  framework:
    type: esp-idf
i2s_audio:
  i2s_bclk_pin: GPIO5
  i2s_lrclk_pin: GPIO25
microphone:
  - platform: i2s_audio
    id: raw_mic
    adc_type: external
    i2s_din_pin: GPIO35
    sample_rate: {source_rate}
    bits_per_sample: 16bit
    channel: stereo
  - platform: resampler
    id: voice_mic
    microphone:
      microphone: raw_mic
      bits_per_sample: 16
      channels: {channels}
    sample_rate: {target_rate}
    fast_decimation: true
""",
        encoding="utf-8",
    )
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    errors = [str(error) for error in result.errors]
    if expected_error is None:
        assert errors == []
    else:
        assert any(expected_error in error for error in errors), errors


@pytest.mark.parametrize(
    ("variant", "framework", "fast", "expected_error"),
    [
        ("esp32", "esp-idf", "true", None),
        ("esp32", "esp-idf", "false", None),
        ("esp32", "arduino", "true", "requires the ESP-IDF framework"),
        ("esp32s3", "esp-idf", "true", "supports only classic ESP32"),
    ],
)
def test_fast_decimation_framework_validation(
    variant: str,
    framework: str,
    fast: str,
    expected_error: str | None,
    tmp_path: Path,
) -> None:
    path = tmp_path / "fast-decimation.yaml"
    path.write_text(
        f"""
esphome:
  name: fast-decimation-test
esp32:
  variant: {variant}
  framework:
    type: {framework}
i2s_audio:
  i2s_bclk_pin: GPIO5
  i2s_lrclk_pin: GPIO21
microphone:
  - platform: i2s_audio
    id: raw_mic
    adc_type: external
    i2s_din_pin: GPIO4
    sample_rate: 48000
    bits_per_sample: 16bit
    channel: stereo
  - platform: resampler
    id: voice_mic
    microphone:
      microphone: raw_mic
      bits_per_sample: 16
      channels: 0
    sample_rate: 16000
    fast_decimation: {fast}
""",
        encoding="utf-8",
    )
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    errors = [str(error) for error in result.errors]
    if expected_error is None:
        assert errors == []
    else:
        assert any(expected_error in error for error in errors), errors


def test_dsp_decimator_chunking_and_reset(tmp_path: Path) -> None:
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is required for the host DSP wrapper test")

    root = Path(__file__).resolve().parents[3]
    mock_header = tmp_path / "dsps_fir.h"
    mock_header.write_text(
        r"""
#pragma once
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <thread>

#define ESP_OK 0
inline std::atomic<bool> fir_active{false};
inline std::atomic<bool> pause_next_fir{false};
inline std::atomic<bool> fir_paused{false};
inline std::atomic<bool> resume_fir{false};
inline std::atomic<int> reset_count{0};
struct fir_f32_t {
  const float *coefficients;
  float *delay;
};
inline int dsps_fird_init_f32(fir_f32_t *fir, const float *coefficients, float *delay,
                              int taps, int decimation) {
  assert(!fir_active.load());
  ++reset_count;
  assert(taps == 64 && decimation == 3);
  fir->coefficients = coefficients;
  fir->delay = delay;
  std::fill(delay, delay + 64, 0.0f);
  return ESP_OK;
}
inline void dsps_fird_f32_ae32(fir_f32_t *fir, const float *input, float *output, int count) {
  fir_active.store(true);
  if (pause_next_fir.exchange(false)) {
    fir_paused.store(true);
    while (!resume_fir.load())
      std::this_thread::yield();
  }
  for (int frame = 0; frame < count; ++frame) {
    for (int i = 0; i < 61; ++i)
      fir->delay[i] = fir->delay[i + 3];
    for (int i = 0; i < 3; ++i)
      fir->delay[61 + i] = input[frame * 3 + i];
    float sum = 0.0f;
    for (int tap = 0; tap < 64; ++tap)
      sum += fir->coefficients[tap] * fir->delay[tap];
    output[frame] = sum;
  }
  fir_active.store(false);
}
""",
        encoding="utf-8",
    )
    source = tmp_path / "check_decimator.cpp"
    binary = tmp_path / "check_decimator"
    source.write_text(
        r"""
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <thread>
#include <vector>

#include "esphome/components/audio/decimator_3.h"

using esphome::audio::DspDecimator3;

std::vector<uint8_t> sine(double frequency) {
  std::vector<uint8_t> bytes;
  for (int i = 0; i < 48000; ++i) {
    int16_t sample = static_cast<int16_t>(12000 * std::sin(2 * std::numbers::pi * frequency * i / 48000));
    bytes.push_back(static_cast<uint8_t>(sample));
    bytes.push_back(static_cast<uint8_t>(sample >> 8));
  }
  return bytes;
}

double rms(const std::vector<uint8_t> &bytes) {
  double sum = 0;
  for (size_t i = 1000; i < bytes.size() / 2; ++i) {
    const auto sample = static_cast<int16_t>(static_cast<uint16_t>(bytes[2 * i]) |
                                             (static_cast<uint16_t>(bytes[2 * i + 1]) << 8));
    sum += static_cast<double>(sample) * sample;
  }
  return std::sqrt(sum / (bytes.size() / 2 - 1000));
}

int main() {
  const auto input = sine(1000);
  DspDecimator3 whole;
  assert(whole.reset());
  std::vector<uint8_t> reference;
  whole.process(input, reference);
  assert(reference.size() == 32000);
  assert(rms(reference) > 8000);

  // PCM callers can pass s16 samples directly without a byte copy.
  std::vector<int16_t> pcm(input.size() / 2);
  for (size_t i = 0; i < pcm.size(); ++i)
    pcm[i] = static_cast<int16_t>(static_cast<uint16_t>(input[2 * i]) |
                                  (static_cast<uint16_t>(input[2 * i + 1]) << 8));
  DspDecimator3 pcm_filter;
  assert(pcm_filter.reset());
  std::vector<int16_t> pcm_output(reference.size() / 2);
  size_t generated = 0;
  assert(!pcm_filter.process(pcm.data(), pcm.size(), pcm_output.data(), pcm_output.size() - 1, generated));
  assert(pcm_filter.process(pcm.data(), pcm.size(), pcm_output.data(), pcm_output.size(), generated));
  assert(generated == pcm_output.size());
  for (size_t i = 0; i < generated; ++i) {
    const auto expected = static_cast<int16_t>(static_cast<uint16_t>(reference[2 * i]) |
                                               (static_cast<uint16_t>(reference[2 * i + 1]) << 8));
    assert(pcm_output[i] == expected);
  }

  DspDecimator3 pcm_chunked;
  assert(pcm_chunked.reset());
  std::vector<int16_t> reconstructed_pcm;
  for (size_t start = 0; start < pcm.size();) {
    const size_t count = std::min<size_t>(1 + start % 479, pcm.size() - start);
    std::vector<int16_t> block((count + 4) / 3);
    size_t block_generated = 0;
    assert(pcm_chunked.process(pcm.data() + start, count, block.data(), block.size(), block_generated));
    reconstructed_pcm.insert(reconstructed_pcm.end(), block.begin(), block.begin() + block_generated);
    start += count;
  }
  assert(reconstructed_pcm == pcm_output);

  DspDecimator3 chunked;
  assert(chunked.reset());
  std::vector<uint8_t> reconstructed;
  for (size_t start = 0; start < input.size();) {
    const size_t end = std::min(start + 2 * (1 + start % 31), input.size());
    std::vector<uint8_t> part(input.begin() + start, input.begin() + end);
    std::vector<uint8_t> out;
    chunked.process(part, out);
    reconstructed.insert(reconstructed.end(), out.begin(), out.end());
    start = end;
  }
  assert(reconstructed == reference);

  // Slightly uneven callback slices also keep the reused output buffer
  // inside its 16 ms setup-time reservation.
  DspDecimator3 bounded;
  assert(bounded.reset());
  std::vector<uint8_t> block_output;
  block_output.reserve(512);
  std::vector<uint8_t> bounded_result;
  for (size_t offset = 0; offset < input.size(); offset += 1532) {
    const size_t bytes = std::min<size_t>(1532, input.size() - offset);
    assert(bounded.process(input.data() + offset, bytes, block_output));
    assert(block_output.capacity() == 512);
    bounded_result.insert(bounded_result.end(), block_output.begin(), block_output.end());
  }
  assert(bounded_result == reference);

  assert(chunked.reset());
  std::vector<uint8_t> after_reset;
  chunked.process(input, after_reset);
  assert(after_reset == reference);

  std::vector<uint8_t> short_input(input.begin(), input.begin() + 1536);
  std::vector<uint8_t> in_flight;
  pause_next_fir.store(true);
  std::thread worker([&] { assert(chunked.process(short_input, in_flight)); });
  while (!fir_paused.load())
    std::this_thread::yield();
  const int resets_before_request = reset_count.load();
  chunked.request_reset();
  assert(reset_count.load() == resets_before_request);
  resume_fir.store(true);
  worker.join();
  assert(reset_count.load() == resets_before_request);

  std::vector<uint8_t> after_restart;
  assert(chunked.process(short_input, after_restart));
  assert(reset_count.load() == resets_before_request + 1);
  DspDecimator3 fresh;
  assert(fresh.reset());
  std::vector<uint8_t> fresh_output;
  assert(fresh.process(short_input, fresh_output));
  assert(after_restart == fresh_output);

  DspDecimator3 anti_alias;
  assert(anti_alias.reset());
  std::vector<uint8_t> stop_band;
  anti_alias.process(sine(12000), stop_band);
  assert(stop_band.size() == 32000);
  assert(rms(stop_band) < rms(reference) / 100);

  DspDecimator3 full_scale;
  assert(full_scale.reset());
  std::vector<uint8_t> maximum_input(96000, 0x7f);
  for (size_t i = 0; i < maximum_input.size(); i += 2)
    maximum_input[i] = 0xff;
  std::vector<uint8_t> maximum_output;
  full_scale.process(maximum_input, maximum_output);
  for (size_t i = 256; i < maximum_output.size(); i += 2) {
    const int16_t sample = static_cast<int16_t>(static_cast<uint16_t>(maximum_output[i]) |
                                                 (static_cast<uint16_t>(maximum_output[i + 1]) << 8));
    assert(sample > 30000);
  }
}
""",
        encoding="utf-8",
    )
    subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-O2",
            "-pthread",
            "-DUSE_DSP_DECIMATOR",
            "-DUSE_ESP_IDF",
            "-I",
            str(tmp_path),
            "-I",
            str(root),
            str(source),
            "-o",
            str(binary),
        ],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
