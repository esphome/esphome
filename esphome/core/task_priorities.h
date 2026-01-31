#pragma once

#ifdef USE_ESP32

#include <freertos/FreeRTOS.h>

namespace esphome {

/// @brief FreeRTOS task priority definitions for ESPHome
///
/// All priorities use relative values based on configMAX_PRIORITIES so they
/// scale automatically if CONFIG_FREERTOS_MAX_PRIORITIES changes.
///
/// Priority hierarchy (with CONFIG_FREERTOS_MAX_PRIORITIES = 16):
///
///   14: Audio capture (I2S microphone) - highest, real-time audio input
///   10: Audio output (I2S speaker) - real-time audio output
///    8: Network stack (LWIP TCP/IP) - set via CONFIG_LWIP_TCPIP_TASK_PRIO
///    6: Audio mixing - buffered audio processing
///    5: Protocol tasks (MQTT, USB host, OpenThread) - communication
///    4: USB serial TX - serial communication
///    3: ML inference (wake word) - background ML processing
///    1: Application (main loop, media pipelines, camera, UART RX) - baseline
///    0: Idle task (FreeRTOS system)
///
/// Guidelines:
/// - Real-time audio I/O tasks need highest priorities to prevent glitches
/// - Network/protocol tasks should be above application but below audio
/// - Background processing (ML, media decoding) can run at low priority

/// Audio capture task priority (I2S microphone)
/// Highest application priority - audio input cannot tolerate delays
static constexpr UBaseType_t TASK_PRIORITY_AUDIO_CAPTURE = configMAX_PRIORITIES - 2;

/// Audio output task priority (I2S speaker)
/// High priority - audio output needs consistent timing
static constexpr UBaseType_t TASK_PRIORITY_AUDIO_OUTPUT = configMAX_PRIORITIES - 6;

/// Audio mixer task priority
/// Medium-high - mixing is buffered but feeds real-time output
static constexpr UBaseType_t TASK_PRIORITY_AUDIO_MIXER = configMAX_PRIORITIES - 10;

/// Protocol/communication task priority (MQTT, USB host, OpenThread)
/// Above application tasks for responsive network handling
static constexpr UBaseType_t TASK_PRIORITY_PROTOCOL = configMAX_PRIORITIES - 11;

/// USB serial TX task priority
/// Slightly below protocol tasks
static constexpr UBaseType_t TASK_PRIORITY_USB_SERIAL = configMAX_PRIORITIES - 12;

/// ML inference task priority (wake word detection)
/// Background processing - can yield to communication tasks
static constexpr UBaseType_t TASK_PRIORITY_INFERENCE = configMAX_PRIORITIES - 13;

/// Application task priority (main loop, media pipelines, camera, etc.)
/// Baseline priority - just above idle task
static constexpr UBaseType_t TASK_PRIORITY_APPLICATION = tskIDLE_PRIORITY + 1;

}  // namespace esphome

#endif  // USE_ESP32
