#ifdef USE_ESP32_VARIANT_ESP32

#include "esp32_touch.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <algorithm>
#include <cinttypes>

// Include HAL for ISR-safe touch reading
#include "hal/touch_sensor_ll.h"

namespace esphome {
namespace esp32_touch {

static const char *const TAG = "esp32_touch";

static const uint32_t SETUP_MODE_THRESHOLD = 0xFFFF;

void ESP32TouchComponent::setup() {
  // Create queue for touch events
  // Queue size calculation: children * 4 allows for burst scenarios where ISR
  // fires multiple times before main loop processes. This is important because
  // ESP32 v1 scans all pads on each interrupt, potentially sending multiple events.
  if (!this->create_touch_queue_()) {
    return;
  }

  touch_pad_init();
  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);

  // Set up IIR filter if enabled
  if (this->iir_filter_enabled_()) {
    touch_pad_filter_start(this->iir_filter_);
  }

  // Configure measurement parameters
#if ESP_IDF_VERSION_MAJOR >= 5
  touch_pad_set_measurement_clock_cycles(this->meas_cycle_);
  touch_pad_set_measurement_interval(this->sleep_cycle_);
#else
  touch_pad_set_meas_time(this->sleep_cycle_, this->meas_cycle_);
#endif
  touch_pad_set_voltage(this->high_voltage_reference_, this->low_voltage_reference_, this->voltage_attenuation_);

  // Configure each touch pad
  for (auto *child : this->children_) {
    if (this->setup_mode_) {
      touch_pad_config(child->get_touch_pad(), SETUP_MODE_THRESHOLD);
    } else {
      touch_pad_config(child->get_touch_pad(), child->get_threshold());
    }
  }

  // Register ISR handler
  esp_err_t err = touch_pad_isr_register(touch_isr_handler, this);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register touch ISR: %s", esp_err_to_name(err));
    this->cleanup_touch_queue_();
    this->mark_failed();
    return;
  }

  // Calculate release timeout based on sleep cycle
  this->calculate_release_timeout_();

  // Enable touch pad interrupt
  touch_pad_intr_enable();
}

void ESP32TouchComponent::dump_config() {
  this->dump_config_base_();

  if (this->iir_filter_enabled_()) {
    ESP_LOGCONFIG(TAG, "    IIR Filter: %" PRIu32 "ms", this->iir_filter_);
  } else {
    ESP_LOGCONFIG(TAG, "  IIR Filter DISABLED");
  }

  if (this->setup_mode_) {
    ESP_LOGCONFIG(TAG, "  Setup Mode ENABLED");
  }

  this->dump_config_sensors_();
}

void ESP32TouchComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // Print debug info for all pads in setup mode
  this->process_setup_mode_logging_(now);

  // Process any queued touch events from interrupts
  // Note: Events are only sent by ISR for pads that were measured in that cycle (value != 0)
  // This is more efficient than sending all pad states every interrupt
  TouchPadEventV1 event;
  while (xQueueReceive(this->touch_queue_, &event, 0) == pdTRUE) {
    // Find the corresponding sensor - O(n) search is acceptable since events are infrequent
    for (auto *child : this->children_) {
      if (child->get_touch_pad() != event.pad) {
        continue;
      }

      // Found matching pad - process it
      child->value_ = event.value;

      // The interrupt gives us the touch state directly
      bool new_state = event.is_touched;

      // Track when we last saw this pad as touched
      if (new_state) {
        child->last_touch_time_ = now;
      }

      // Only publish if state changed - this filters out repeated events
      if (new_state != child->last_state_) {
        child->initial_state_published_ = true;
        child->last_state_ = new_state;
        child->publish_state(new_state);
        // Original ESP32: ISR only fires when touched, release is detected by timeout
        // Note: ESP32 v1 uses inverted logic - touched when value < threshold
        ESP_LOGV(TAG, "Touch Pad '%s' state: %s (value: %" PRIu32 " < threshold: %" PRIu32 ")",
                 child->get_name().c_str(), ONOFF(new_state), event.value, child->get_threshold());
      }
      break;  // Exit inner loop after processing matching pad
    }
  }

  // Check for released pads periodically
  if (!this->should_check_for_releases_(now)) {
    return;
  }

  size_t pads_off = 0;
  for (auto *child : this->children_) {
    // Handle initial state publication after startup
    this->publish_initial_state_if_needed_(child, now);

    if (child->last_state_) {
      // Pad is currently in touched state - check for release timeout
      // Using subtraction handles 32-bit rollover correctly
      uint32_t time_diff = now - child->last_touch_time_;

      // Check if we haven't seen this pad recently
      if (time_diff > this->release_timeout_ms_) {
        // Haven't seen this pad recently, assume it's released
        child->last_state_ = false;
        child->publish_state(false);
        ESP_LOGV(TAG, "Touch Pad '%s' state: OFF (timeout)", child->get_name().c_str());
        pads_off++;
      }
    } else {
      // Pad is already off
      pads_off++;
    }
  }

  // Disable the loop to save CPU cycles when all pads are off and not in setup mode.
  // The loop will be re-enabled by the ISR when any touch pad is touched.
  // v1 hardware limitations require us to check all pads are off because:
  // - v1 only generates interrupts on touch events (not releases)
  // - We must poll for release timeouts in the main loop
  // - We can only safely disable when no pads need timeout monitoring
  this->check_and_disable_loop_if_all_released_(pads_off);
}

void ESP32TouchComponent::on_shutdown() {
  touch_pad_intr_disable();
  touch_pad_isr_deregister(touch_isr_handler, this);
  this->cleanup_touch_queue_();

  if (this->iir_filter_enabled_()) {
    touch_pad_filter_stop();
    touch_pad_filter_delete();
  }

  // Configure wakeup pads if any are set
  this->configure_wakeup_pads_();
}

void IRAM_ATTR ESP32TouchComponent::touch_isr_handler(void *arg) {
  ESP32TouchComponent *component = static_cast<ESP32TouchComponent *>(arg);

  uint32_t mask = 0;
  touch_ll_read_trigger_status_mask(&mask);
  touch_ll_clear_trigger_status_mask();
  touch_pad_clear_status();

  // INTERRUPT BEHAVIOR: On ESP32 v1 hardware, the interrupt fires when ANY configured
  // touch pad detects a touch (value goes below threshold). The hardware does NOT
  // generate interrupts on release - only on touch events.
  // The interrupt will continue to fire periodically (based on sleep_cycle) as long
  // as any pad remains touched. This allows us to detect both new touches and
  // continued touches, but releases must be detected by timeout in the main loop.

  // Process all configured pads to check their current state
  // Note: ESP32 v1 doesn't tell us which specific pad triggered the interrupt,
  // so we must scan all configured pads to find which ones were touched
  for (auto *child : component->children_) {
    touch_pad_t pad = child->get_touch_pad();

    // Read current value using ISR-safe API
    // IMPORTANT: ESP-IDF v5.4 regression - touch_pad_read_filtered() is no longer ISR-safe
    // In ESP-IDF v5.3 and earlier it was ISR-safe, but ESP-IDF v5.4 added mutex protection that causes:
    // "assert failed: xQueueSemaphoreTake queue.c:1718"
    // We must use raw values even when filter is enabled as a workaround.
    // Users should adjust thresholds to compensate for the lack of IIR filtering.
    // See: https://github.com/espressif/esp-idf/issues/17045
    uint32_t value = touch_ll_read_raw_data(pad);

    // Skip pads that aren’t in the trigger mask
    if (((mask >> pad) & 1) == 0) {
      continue;
    }

    // IMPORTANT: ESP32 v1 touch detection logic - INVERTED compared to v2!
    // ESP32 v1: Touch is detected when capacitance INCREASES, causing the measured value to DECREASE
    // Therefore: touched = (value < threshold)
    // This is opposite to ESP32-S2/S3 v2 where touched = (value > threshold)
    bool is_touched = value < child->get_threshold();

    // Always send the current state - the main loop will filter for changes
    // We send both touched and untouched states because the ISR doesn't
    // track previous state (to keep ISR fast and simple)
    TouchPadEventV1 event;
    event.pad = pad;
    event.value = value;
    event.is_touched = is_touched;

    // Send to queue from ISR - non-blocking, drops if queue full
    BaseType_t x_higher_priority_task_woken = pdFALSE;
    xQueueSendFromISR(component->touch_queue_, &event, &x_higher_priority_task_woken);
    component->enable_loop_soon_any_context();
    if (x_higher_priority_task_woken) {
      portYIELD_FROM_ISR();
    }
  }
}

}  // namespace esp32_touch
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32
