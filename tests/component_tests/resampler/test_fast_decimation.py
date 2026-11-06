"""Host-side checks for the integer-ratio microphone decimator."""

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
        (48000, 24000, "0", None),
        (44100, 22050, "0", None),
        (32000, 16000, "0", None),
        *[(ratio * 8000, 8000, "0", None) for ratio in range(2, 17)],
        (48000, 32000, "0", "integer source/output sample-rate ratio"),
        (48000, 16000, "[0, 1]", "mono 16-bit microphone input"),
        (48000, 10000, "0", "integer source/output sample-rate ratio"),
        (16000, 16000, "0", "integer source/output sample-rate ratio"),
        (136000, 8000, "0", "integer source/output sample-rate ratio"),
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
  int taps;
  int decimation;
};
inline int dsps_fird_init_f32(fir_f32_t *fir, const float *coefficients, float *delay,
                              int taps, int decimation) {
  assert(!fir_active.load());
  ++reset_count;
  assert(taps >= 64 && taps <= 512 && taps % 4 == 0);
  assert(decimation >= 2 && decimation <= 16);
  fir->coefficients = coefficients;
  fir->delay = delay;
  fir->taps = taps;
  fir->decimation = decimation;
  std::fill(delay, delay + taps, 0.0f);
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
    for (int i = 0; i < fir->taps - fir->decimation; ++i)
      fir->delay[i] = fir->delay[i + fir->decimation];
    for (int i = 0; i < fir->decimation; ++i)
      fir->delay[fir->taps - fir->decimation + i] = input[frame * fir->decimation + i];
    float sum = 0.0f;
    for (int tap = 0; tap < fir->taps; ++tap)
      sum += fir->coefficients[tap] * fir->delay[tap];
    output[frame] = sum;
  }
  fir_active.store(false);
}
""",
        encoding="utf-8",
    )
    (tmp_path / "esp_heap_caps.h").write_text(
        r"""
#pragma once
#include <atomic>
#include <cassert>
#include <cstdlib>

#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
inline std::atomic<int> filter_allocations{0};
inline std::atomic<int> filter_frees{0};
inline std::atomic<bool> fail_next_allocation{false};
inline void *heap_caps_malloc(size_t bytes, int capabilities) {
  assert(capabilities == (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  assert(bytes >= 2 * 96 * sizeof(float) && bytes <= 2 * 512 * sizeof(float));
  if (fail_next_allocation.exchange(false))
    return nullptr;
  ++filter_allocations;
  return std::malloc(bytes);
}
inline void heap_caps_free(void *pointer) {
  if (pointer != nullptr)
    ++filter_frees;
  std::free(pointer);
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
#include <utility>
#include <vector>

#include "esphome/components/audio/decimator.h"

using esphome::audio::DspDecimator;

static_assert(sizeof(DspDecimator) < 4096);

std::vector<uint8_t> sine(double frequency, int sample_rate = 48000) {
  std::vector<uint8_t> bytes;
  for (int i = 0; i < sample_rate; ++i) {
    int16_t sample = static_cast<int16_t>(12000 * std::sin(2 * std::numbers::pi * frequency * i / sample_rate));
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
  DspDecimator whole;
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
  DspDecimator pcm_filter;
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

  DspDecimator pcm_chunked;
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

  DspDecimator chunked;
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
  DspDecimator bounded;
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
  DspDecimator fresh;
  assert(fresh.reset());
  std::vector<uint8_t> fresh_output;
  assert(fresh.process(short_input, fresh_output));
  assert(after_restart == fresh_output);

  DspDecimator anti_alias;
  assert(anti_alias.reset());
  std::vector<uint8_t> stop_band;
  anti_alias.process(sine(12000), stop_band);
  assert(stop_band.size() == 32000);
  assert(rms(stop_band) < rms(reference) / 100);

  DspDecimator full_scale;
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

  // The same API selects a frequency-appropriate anti-alias filter for
  // every supported integer ratio, including non-48 kHz source clocks.
  std::vector<std::pair<int, int>> ratios = {{44100, 22050}, {48000, 16000}};
  for (int ratio = 2; ratio <= 16; ++ratio)
    ratios.emplace_back(ratio * 8000, 8000);
  for (const auto [source_rate, target_rate] : ratios) {
    const int allocations_before = filter_allocations.load();
    DspDecimator pass_filter;
    assert(pass_filter.configure(source_rate, target_rate));
    const int allocations_after_configure = filter_allocations.load();
    assert(allocations_after_configure - allocations_before == (source_rate / target_rate > 3 ? 1 : 0));
    const auto pass_input = sine(target_rate / 10.0, source_rate);
    std::vector<uint8_t> pass_output;
    assert(pass_filter.process(pass_input, pass_output));
    assert(filter_allocations.load() == allocations_after_configure);
    assert(pass_output.size() == static_cast<size_t>(target_rate) * sizeof(int16_t));
    assert(rms(pass_output) > 8000);

    std::vector<uint8_t> upper_passband;
    assert(pass_filter.process(sine(target_rate * 0.35, source_rate), upper_passband));
    assert(rms(upper_passband) > 7000);

    DspDecimator stop_filter;
    assert(stop_filter.configure(source_rate, target_rate));
    std::vector<uint8_t> stop_output;
    // The first stop-band tone is immediately above the output Nyquist
    // frequency; it must not alias into the audible output passband.
    assert(stop_filter.process(sine(target_rate * 0.51, source_rate), stop_output));
    assert(stop_output.size() == pass_output.size());
    assert(rms(stop_output) < rms(pass_output) / 100);

    DspDecimator sliced_filter;
    assert(sliced_filter.configure(source_rate, target_rate));
    std::vector<uint8_t> sliced_output;
    for (size_t start = 0; start < pass_input.size();) {
      const size_t end = std::min(start + 2 * (1 + start % 37), pass_input.size());
      std::vector<uint8_t> block;
      assert(sliced_filter.process(pass_input.data() + start, end - start, block));
      sliced_output.insert(sliced_output.end(), block.begin(), block.end());
      start = end;
    }
    assert(sliced_output == pass_output);
  }
  DspDecimator invalid;
  assert(!invalid.configure(48000, 44100));
  assert(!invalid.configure(48000, 0));
  assert(!invalid.configure(136000, 8000));
  fail_next_allocation.store(true);
  assert(!invalid.configure(128000, 8000));
  assert(invalid.configure(128000, 8000));
  const int allocations_after_high_ratio = filter_allocations.load();
  assert(invalid.reset());
  assert(filter_allocations.load() == allocations_after_high_ratio);
  assert(invalid.configure(48000, 16000));
  assert(filter_allocations.load() == filter_frees.load());
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
