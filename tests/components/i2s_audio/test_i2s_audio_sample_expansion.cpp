#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "esphome/components/i2s_audio/speaker/i2s_audio_sample_conversion.h"
#include <pcm_convert.h>

namespace esphome::i2s_audio::testing {

TEST(I2SAudioOutputWidth, PreservesInputWhenExpansionIsDisabled) {
  EXPECT_EQ(resolve_i2s_output_bits(16, 32, false), 16);
}

TEST(I2SAudioOutputWidth, ExpandsNarrowerInputToExplicitSlot) { EXPECT_EQ(resolve_i2s_output_bits(16, 32, true), 32); }

TEST(I2SAudioOutputWidth, PreservesEqualWidth) { EXPECT_EQ(resolve_i2s_output_bits(32, 32, true), 32); }

TEST(I2SAudioOutputWidth, AutoSlotPreservesInput) {
  EXPECT_EQ(resolve_i2s_output_bits(16, I2S_SLOT_BITS_AUTO, true), 16);
}

TEST(I2SAudioOutputWidth, NarrowsInputWiderThanSlot) {
  EXPECT_EQ(resolve_i2s_output_bits(32, 16, false), 16);
  EXPECT_EQ(resolve_i2s_output_bits(32, 16, true), 16);
}

TEST(I2SAudioSampleExpansion, ExpandsStereoFramesAndReportsAccounting) {
  std::array<uint8_t, 8> input{
      0x34, 0x12,  // frame 1 left: 0x1234
      0xCC, 0xED,  // frame 1 right: -0x1234
      0x00, 0x00,  // frame 2 left: silence
      0x02, 0x01,  // frame 2 right: 0x0102
  };
  const auto original_input = input;
  std::array<uint8_t, 16> scratch;
  scratch.fill(0xAA);

  const I2SAudioExpansion result = prepare_i2s_audio_expansion(scratch.data(), 2, 4, 2, 2);
  esp_audio_libs::pcm_convert::copy_frames(input.data(), result.output_data, 2, 2, 4, 2, result.frames);

  const std::array<uint8_t, 16> expected{
      0x00, 0x00, 0x34, 0x12,  // frame 1 left
      0x00, 0x00, 0xCC, 0xED,  // frame 1 right
      0x00, 0x00, 0x00, 0x00,  // frame 2 left
      0x00, 0x00, 0x02, 0x01,  // frame 2 right
  };
  EXPECT_EQ(result.output_data, scratch.data());
  EXPECT_EQ(result.frames, 2);
  EXPECT_EQ(result.input_bytes, input.size());
  EXPECT_EQ(result.output_bytes, scratch.size());
  EXPECT_EQ(scratch, expected);
  EXPECT_EQ(input, original_input);
}

}  // namespace esphome::i2s_audio::testing
