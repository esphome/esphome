#ifdef USE_HOST
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
// Empty without USE_ESP32, and included first so the runtime does not pull in FreeRTOS.
#include "esphome/core/main_task.h"
#include "esphome/core/optional.h"
#include "esphome/core/string_ref.h"
#include "tflite_runtime_platform.h"

// Compile the production runtime; only the inference engine and the ESP platform headers use __sdk.
#define USE_ESP32
#ifndef USE_TFLITE_MICRO_HELPER
#define USE_TFLITE_MICRO_HELPER
#endif
// NOLINTNEXTLINE(bugprone-suspicious-include): exercise the ESP-only runtime with the host doubles.
#include "esphome/components/tflite_micro_runtime/tflite_micro_runtime.cpp"

namespace esphome::tflite_micro_runtime::testing {

using tflite_runtime_testing::engine;

class Decoder : public tflite_output_processor::OutputDecoder {
 public:
  bool decode(TfLiteTensor *const *, size_t) override {
    ++this->calls;
    if (this->during_decode)
      this->during_decode();
    return this->succeeds;
  }
  unsigned calls{0};
  bool succeeds{true};
  std::function<void()> during_decode;
};

class Pipeline : public InputPipeline {
 public:
  bool fill(TfLiteTensor *, const uint8_t *, size_t) override {
    ++this->calls;
    return true;
  }
  const char *name() const override { return "test"; }
  unsigned calls{0};
};

class ResultLifecycleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    arch_init();  // this thread is the main loop
    engine = {};
    ASSERT_TRUE(this->helper.set_model(MODEL_DATA, sizeof(MODEL_DATA), 0));
    ASSERT_TRUE(this->helper.load_model());
  }
  void subscribe() {
    this->helper.add_on_result_callback([this]() {
      ++this->notified;
      if (this->during_callback)
        this->during_callback();
    });
  }
  bool run() { return this->helper.run(this->input, sizeof(this->input)); }
  bool run_on_worker() {
    bool succeeded = false;
    std::thread worker([this, &succeeded]() { succeeded = this->run(); });
    worker.join();
    return succeeded;
  }

  static constexpr uint8_t MODEL_DATA[]{0};
  const uint8_t input[4]{};
  TFLiteMicroHelper helper;
  unsigned notified{0};
  std::function<void()> during_callback;
};

TEST_F(ResultLifecycleTest, AMainTaskRunDeliversExactlyOnce) {
  this->subscribe();
  ASSERT_TRUE(this->run());
  EXPECT_EQ(this->notified, 1u);
  this->helper.loop();
  EXPECT_EQ(this->notified, 1u);
}

TEST_F(ResultLifecycleTest, AWorkerResultWaitsForTheMainLoopAndKeepsItsTensors) {
  this->subscribe();
  ASSERT_TRUE(this->run_on_worker());
  EXPECT_EQ(this->notified, 0u);
  EXPECT_FALSE(this->run_on_worker());
  EXPECT_EQ(this->helper.invoke(), kTfLiteError);
  EXPECT_FALSE(this->helper.run_inference_on_buffer(this->input, sizeof(this->input)));
  EXPECT_FALSE(this->helper.unload_model());
  EXPECT_EQ(engine.invocations, 1u);

  this->helper.loop();
  EXPECT_EQ(this->notified, 1u);
  this->helper.loop();
  EXPECT_EQ(this->notified, 1u);
  EXPECT_TRUE(this->run_on_worker());
  this->helper.loop();
  EXPECT_EQ(this->notified, 2u);
  EXPECT_TRUE(this->helper.unload_model());
}

TEST_F(ResultLifecycleTest, ACallbackCannotStartAnotherRunOrUnload) {
  this->subscribe();
  this->during_callback = [this]() {
    EXPECT_FALSE(this->run());
    EXPECT_FALSE(this->helper.run_inference(this->input, sizeof(this->input)).has_value());
    EXPECT_EQ(this->helper.invoke(), kTfLiteError);
    EXPECT_FALSE(this->helper.unload_model());
    EXPECT_EQ(this->helper.get_last_result().value, 1.0f);
  };
  ASSERT_TRUE(this->helper.run_inference(this->input, sizeof(this->input)).has_value());
  EXPECT_EQ(this->notified, 1u);
  EXPECT_EQ(engine.invocations, 1u);
}

TEST_F(ResultLifecycleTest, AFailedRunReleasesTheReservation) {
  this->subscribe();
  engine.invoke_status = kTfLiteError;
  EXPECT_FALSE(this->run());
  EXPECT_FALSE(this->helper.run_inference(this->input, sizeof(this->input)).has_value());
  EXPECT_EQ(this->notified, 0u);
  engine.invoke_status = kTfLiteOk;
  EXPECT_TRUE(this->run());
  EXPECT_EQ(this->notified, 1u);
}

TEST_F(ResultLifecycleTest, ADecoderFailureReleasesTheReservation) {
  Decoder decoder;
  this->helper.set_output_decoder(&decoder);
  this->subscribe();
  decoder.succeeds = false;
  EXPECT_FALSE(this->run());
  EXPECT_EQ(this->notified, 0u);
  decoder.succeeds = true;
  EXPECT_TRUE(this->run());
  EXPECT_EQ(this->notified, 1u);
}

TEST_F(ResultLifecycleTest, ANullPayloadNeverReachesThePipeline) {
  Pipeline pipeline;
  this->helper.set_input_pipeline(&pipeline);
  this->subscribe();
  EXPECT_FALSE(this->helper.run(nullptr, sizeof(this->input)));
  EXPECT_EQ(pipeline.calls, 0u);
  EXPECT_EQ(this->notified, 0u);
  EXPECT_TRUE(this->run());
  EXPECT_EQ(pipeline.calls, 1u);
}

TEST_F(ResultLifecycleTest, ASameTaskReentryIsRejectedInsteadOfDeadlocking) {
  Decoder decoder;
  this->helper.set_output_decoder(&decoder);
  decoder.during_decode = [this]() {
    EXPECT_EQ(this->helper.get_arena_used_bytes(), 0u);
    EXPECT_FALSE(this->run());
  };
  EXPECT_TRUE(this->run());
  EXPECT_EQ(decoder.calls, 1u);
  EXPECT_EQ(this->helper.get_arena_used_bytes(), 128u);
}

TEST_F(ResultLifecycleTest, ADecoderRunDropsTheResultOfAnEarlierRun) {
  Decoder decoder;
  this->helper.set_output_decoder(&decoder);
  this->subscribe();
  ASSERT_TRUE(this->helper.run_inference(this->input, sizeof(this->input)).has_value());
  EXPECT_TRUE(this->helper.get_last_result().valid);
  this->during_callback = [this]() { EXPECT_FALSE(this->helper.get_last_result().valid); };
  ASSERT_TRUE(this->run());
  EXPECT_EQ(this->notified, 2u);
}

TEST_F(ResultLifecycleTest, AnUnmappedOutputTypeIsNotifiedWithAnInvalidResult) {
  this->subscribe();
  ASSERT_TRUE(this->run());
  engine.output_type = kTfLiteInt32;
  const auto result = this->helper.run_inference(this->input, sizeof(this->input));
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->valid);
  EXPECT_FALSE(this->helper.get_last_result().valid);
  ASSERT_TRUE(this->run());
  EXPECT_FALSE(this->helper.get_last_result().valid);
  EXPECT_EQ(this->notified, 3u);
}

TEST_F(ResultLifecycleTest, AFailedLoadReleasesTheArenaAndCanBeRetried) {
  ASSERT_TRUE(this->helper.unload_model());
  engine.load_succeeds = false;
  const unsigned unloads = engine.unloads;
  EXPECT_FALSE(this->helper.load_model());
  EXPECT_FALSE(this->helper.is_model_loaded());
  EXPECT_EQ(engine.unloads, unloads + 1);
  EXPECT_EQ(this->helper.get_tensor_arena_size_actual(), 0u);
  EXPECT_FALSE(this->helper.get_last_load_stats().success);
  EXPECT_FALSE(this->run());
  engine.load_succeeds = true;
  EXPECT_TRUE(this->helper.load_model());
  EXPECT_TRUE(this->run());
}

TEST_F(ResultLifecycleTest, AFailedArenaAllocationFailsTheLoad) {
  ASSERT_TRUE(this->helper.unload_model());
  engine.allocation_succeeds = false;
  EXPECT_FALSE(this->helper.load_model());
  EXPECT_FALSE(this->helper.is_model_loaded());
  engine.allocation_succeeds = true;
  EXPECT_TRUE(this->helper.load_model());
}

TEST_F(ResultLifecycleTest, AReloadAfterUnloadRunsAgain) {
  ASSERT_TRUE(this->run());
  EXPECT_FALSE(this->helper.load_model());  // already loaded
  ASSERT_TRUE(this->helper.unload_model());
  EXPECT_EQ(this->helper.input_tensor(), nullptr);
  EXPECT_FALSE(this->run());
  ASSERT_TRUE(this->helper.load_model());
  EXPECT_TRUE(this->run());
  EXPECT_EQ(engine.invocations, 2u);
}

TEST_F(ResultLifecycleTest, AnUnloadWaitsForAnInferenceSession) {
  std::atomic<bool> unloaded{false};
  std::thread worker;
  {
    InferenceSession session(&this->helper);
    ASSERT_TRUE(session.valid());
    worker = std::thread([this, &unloaded]() {
      this->helper.unload_model();
      unloaded = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(unloaded);
    EXPECT_EQ(session.invoke(), kTfLiteOk);
    EXPECT_NE(session.output_tensor(), nullptr);
  }
  worker.join();
  EXPECT_TRUE(unloaded);
  EXPECT_FALSE(this->helper.is_model_loaded());
}

}  // namespace esphome::tflite_micro_runtime::testing
#endif
