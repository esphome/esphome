#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

#include "esp32_touch.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace esp32_touch {

static const char *const TAG = "esp32_touch";

// Helper to update touch state with a known state and value
void ESP32TouchComponent::update_touch_state_(ESP32TouchBinarySensor *child, bool is_touched, uint32_t value) {
  // Store the value for get_value() access in lambdas
  child->value_ = value;

  // Always update timer when touched
  if (is_touched) {
    child->last_touch_time_ = App.get_loop_component_start_time();
  }

  if (child->last_state_ != is_touched) {
    child->last_state_ = is_touched;
    child->publish_state(is_touched);
    if (is_touched) {
      ESP_LOGV(TAG, "Touch Pad '%s' state: ON (value: %" PRIu32 " > threshold: %" PRIu32 ")", child->get_name().c_str(),
               value, child->threshold_ + child->benchmark_);
    } else {
      ESP_LOGV(TAG, "Touch Pad '%s' state: OFF", child->get_name().c_str());
    }
  }
}

// Helper to read touch value and update state for a given child (used for timeout events)
bool ESP32TouchComponent::check_and_update_touch_state_(ESP32TouchBinarySensor *child) {
  // Read current touch value
  uint32_t value = this->read_touch_value(child->touch_pad_);

  // ESP32-S2/S3 v2: Touch is detected when value > threshold + benchmark
  ESP_LOGV(TAG,
           "Checking touch state for '%s' (T%d): value = %" PRIu32 ", threshold = %" PRIu32 ", benchmark = %" PRIu32,
           child->get_name().c_str(), child->touch_pad_, value, child->threshold_, child->benchmark_);
  bool is_touched = value > child->benchmark_ + child->threshold_;

  this->update_touch_state_(child, is_touched, value);
  return is_touched;
}

void ESP32TouchComponent::setup() {
  // Create queue for touch events first
  if (!this->create_touch_queue_()) {
    return;
  }

  // Initialize touch pad peripheral
  esp_err_t init_err = touch_pad_init();
  if (init_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize touch pad: %s", esp_err_to_name(init_err));
    this->mark_failed();
    return;
  }

  // Configure each touch pad first
  for (auto *child : this->children_) {
    esp_err_t config_err = touch_pad_config(child->touch_pad_);
    if (config_err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to configure touch pad %d: %s", child->touch_pad_, esp_err_to_name(config_err));
    }
  }

  // Set up filtering if configured
  if (this->filter_configured_()) {
    touch_filter_config_t filter_info = {
        .mode = this->filter_mode_,
        .debounce_cnt = this->debounce_count_,
        .noise_thr = this->noise_threshold_,
        .jitter_step = this->jitter_step_,
        .smh_lvl = this->smooth_level_,
    };
    touch_pad_filter_set_config(&filter_info);
    touch_pad_filter_enable();
  }

  if (this->denoise_configured_()) {
    touch_pad_denoise_t denoise = {
        .grade = this->grade_,
        .cap_level = this->cap_level_,
    };
    touch_pad_denoise_set_config(&denoise);
    touch_pad_denoise_enable();
  }

  if (this->waterproof_configured_()) {
    touch_pad_waterproof_t waterproof = {
        .guard_ring_pad = this->waterproof_guard_ring_pad_,
        .shield_driver = this->waterproof_shield_driver_,
    };
    touch_pad_waterproof_set_config(&waterproof);
    touch_pad_waterproof_enable();
  }

  // Configure measurement parameters
  touch_pad_set_voltage(this->high_voltage_reference_, this->low_voltage_reference_, this->voltage_attenuation_);
  touch_pad_set_charge_discharge_times(this->meas_cycle_);
  touch_pad_set_measurement_interval(this->sleep_cycle_);

  // Configure timeout if needed
  touch_pad_timeout_set(true, TOUCH_PAD_THRESHOLD_MAX);

  // Register ISR handler with interrupt mask
  esp_err_t err =
      touch_pad_isr_register(touch_isr_handler, this, static_cast<touch_pad_intr_mask_t>(TOUCH_PAD_INTR_MASK_ALL));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register touch ISR: %s", esp_err_to_name(err));
    this->cleanup_touch_queue_();
    this->mark_failed();
    return;
  }

  // Set thresholds for each pad BEFORE starting FSM
  for (auto *child : this->children_) {
    if (child->threshold_ != 0) {
      touch_pad_set_thresh(child->touch_pad_, child->threshold_);
    }
  }

  // Enable interrupts - only ACTIVE and TIMEOUT
  // NOTE: We intentionally don't enable INACTIVE interrupts because they are unreliable
  // on ESP32-S2/S3 hardware and sometimes don't fire. Instead, we use timeout-based
  // release detection with the ability to verify the actual state.
  touch_pad_intr_enable(static_cast<touch_pad_intr_mask_t>(TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_TIMEOUT));

  // Set FSM mode before starting
  touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);

  // Start FSM
  touch_pad_fsm_start();

  // Calculate release timeout based on sleep cycle
  this->calculate_release_timeout_();
}

void ESP32TouchComponent::dump_config() {
  this->dump_config_base_();

  if (this->filter_configured_()) {
    const char *filter_mode_s;
    switch (this->filter_mode_) {
      case TOUCH_PAD_FILTER_IIR_4:
        filter_mode_s = "IIR_4";
        break;
      case TOUCH_PAD_FILTER_IIR_8:
        filter_mode_s = "IIR_8";
        break;
      case TOUCH_PAD_FILTER_IIR_16:
        filter_mode_s = "IIR_16";
        break;
      case TOUCH_PAD_FILTER_IIR_32:
        filter_mode_s = "IIR_32";
        break;
      case TOUCH_PAD_FILTER_IIR_64:
        filter_mode_s = "IIR_64";
        break;
      case TOUCH_PAD_FILTER_IIR_128:
        filter_mode_s = "IIR_128";
        break;
      case TOUCH_PAD_FILTER_IIR_256:
        filter_mode_s = "IIR_256";
        break;
      case TOUCH_PAD_FILTER_JITTER:
        filter_mode_s = "JITTER";
        break;
      default:
        filter_mode_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG,
                  "  Filter mode: %s\n"
                  "  Debounce count: %" PRIu32 "\n"
                  "  Noise threshold coefficient: %" PRIu32 "\n"
                  "  Jitter filter step size: %" PRIu32,
                  filter_mode_s, this->debounce_count_, this->noise_threshold_, this->jitter_step_);
    const char *smooth_level_s;
    switch (this->smooth_level_) {
      case TOUCH_PAD_SMOOTH_OFF:
        smooth_level_s = "OFF";
        break;
      case TOUCH_PAD_SMOOTH_IIR_2:
        smooth_level_s = "IIR_2";
        break;
      case TOUCH_PAD_SMOOTH_IIR_4:
        smooth_level_s = "IIR_4";
        break;
      case TOUCH_PAD_SMOOTH_IIR_8:
        smooth_level_s = "IIR_8";
        break;
      default:
        smooth_level_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG, "  Smooth level: %s", smooth_level_s);
  }

  if (this->denoise_configured_()) {
    const char *grade_s;
    switch (this->grade_) {
      case TOUCH_PAD_DENOISE_BIT12:
        grade_s = "BIT12";
        break;
      case TOUCH_PAD_DENOISE_BIT10:
        grade_s = "BIT10";
        break;
      case TOUCH_PAD_DENOISE_BIT8:
        grade_s = "BIT8";
        break;
      case TOUCH_PAD_DENOISE_BIT4:
        grade_s = "BIT4";
        break;
      default:
        grade_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG, "  Denoise grade: %s", grade_s);

    const char *cap_level_s;
    switch (this->cap_level_) {
      case TOUCH_PAD_DENOISE_CAP_L0:
        cap_level_s = "L0";
        break;
      case TOUCH_PAD_DENOISE_CAP_L1:
        cap_level_s = "L1";
        break;
      case TOUCH_PAD_DENOISE_CAP_L2:
        cap_level_s = "L2";
        break;
      case TOUCH_PAD_DENOISE_CAP_L3:
        cap_level_s = "L3";
        break;
      case TOUCH_PAD_DENOISE_CAP_L4:
        cap_level_s = "L4";
        break;
      case TOUCH_PAD_DENOISE_CAP_L5:
        cap_level_s = "L5";
        break;
      case TOUCH_PAD_DENOISE_CAP_L6:
        cap_level_s = "L6";
        break;
      case TOUCH_PAD_DENOISE_CAP_L7:
        cap_level_s = "L7";
        break;
      default:
        cap_level_s = "UNKNOWN";
        break;
    }
    ESP_LOGCONFIG(TAG, "  Denoise capacitance level: %s", cap_level_s);
  }

  if (this->setup_mode_) {
    ESP_LOGCONFIG(TAG, "  Setup Mode ENABLED");
  }

  this->dump_config_sensors_();
}

void ESP32TouchComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // V2 TOUCH HANDLING:
  // Due to unreliable INACTIVE interrupts on ESP32-S2/S3, we use a hybrid approach:
  // 1. Process ACTIVE interrupts when pads are touched
  // 2. Use timeout-based release detection (like v1)
  // 3. But smarter than v1: verify actual state before releasing on timeout
  //    This prevents false releases if we missed interrupts

  // In setup mode, periodically log all pad values
  this->process_setup_mode_logging_(now);

  // Process any queued touch events from interrupts
  TouchPadEventV2 event;
  while (xQueueReceive(this->touch_queue_, &event, 0) == pdTRUE) {
    ESP_LOGD(TAG, "Event received, mask = 0x%" PRIx32 ", pad = %d", event.intr_mask, event.pad);
    // Handle timeout events
    if (event.intr_mask & TOUCH_PAD_INTR_MASK_TIMEOUT) {
      // Resume measurement after timeout
      touch_pad_timeout_resume();
      // For timeout events, always check the current state
    } else if (!(event.intr_mask & TOUCH_PAD_INTR_MASK_ACTIVE)) {
      // Skip if not an active/timeout event
      continue;
    }

    // Find the child for the pad that triggered the interrupt
    for (auto *child : this->children_) {
      if (child->touch_pad_ == event.pad) {
        if (event.intr_mask & TOUCH_PAD_INTR_MASK_TIMEOUT) {
          // For timeout events, we need to read the value to determine state
          this->check_and_update_touch_state_(child);
        } else if (event.intr_mask & TOUCH_PAD_INTR_MASK_ACTIVE) {
          // We only get ACTIVE interrupts now, releases are detected by timeout
          // Read the current value
          uint32_t value = this->read_touch_value(child->touch_pad_);
          this->update_touch_state_(child, true, value);  // Always touched for ACTIVE interrupts
        }
        break;
      }
    }
  }

  // Check for released pads periodically (like v1)
  if (!this->should_check_for_releases_(now)) {
    return;
  }

  size_t pads_off = 0;
  for (auto *child : this->children_) {
    if (child->benchmark_ == 0)
      touch_pad_read_benchmark(child->touch_pad_, &child->benchmark_);
    // Handle initial state publication after startup
    this->publish_initial_state_if_needed_(child, now);

    if (child->last_state_) {
      // Pad is currently in touched state - check for release timeout
      // Using subtraction handles 32-bit rollover correctly
      uint32_t time_diff = now - child->last_touch_time_;

      // Check if we haven't seen this pad recently
      if (time_diff > this->release_timeout_ms_) {
        // Haven't seen this pad recently - verify actual state
        // Unlike v1, v2 hardware allows us to read the current state anytime
        // This makes v2 smarter: we can verify if it's actually released before
        // declaring a timeout, preventing false releases if interrupts were missed
        bool still_touched = this->check_and_update_touch_state_(child);

        if (still_touched) {
          // Still touched! Timer was reset in update_touch_state_
          ESP_LOGVV(TAG, "Touch Pad '%s' still touched after %" PRIu32 "ms timeout, resetting timer",
                    child->get_name().c_str(), this->release_timeout_ms_);
        } else {
          // Actually released - already handled by check_and_update_touch_state_
          pads_off++;
        }
      }
    } else {
      // Pad is already off
      pads_off++;
    }
  }

  // Disable the loop when all pads are off and not in setup mode (like v1)
  // We need to keep checking for timeouts, so only disable when all pads are confirmed off
  this->check_and_disable_loop_if_all_released_(pads_off);
}

void ESP32TouchComponent::on_shutdown() {
  // Disable interrupts
  touch_pad_intr_disable(static_cast<touch_pad_intr_mask_t>(TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_TIMEOUT));
  touch_pad_isr_deregister(touch_isr_handler, this);
  this->cleanup_touch_queue_();

  // Configure wakeup pads if any are set
  this->configure_wakeup_pads_();
}

void IRAM_ATTR ESP32TouchComponent::touch_isr_handler(void *arg) {
  ESP32TouchComponent *component = static_cast<ESP32TouchComponent *>(arg);
  BaseType_t x_higher_priority_task_woken = pdFALSE;

  // Read interrupt status
  TouchPadEventV2 event;
  event.intr_mask = touch_pad_read_intr_status_mask();
  event.pad = touch_pad_get_current_meas_channel();

  // Send event to queue for processing in main loop
  xQueueSendFromISR(component->touch_queue_, &event, &x_higher_priority_task_woken);
  component->enable_loop_soon_any_context();

  if (x_higher_priority_task_woken) {
    portYIELD_FROM_ISR();
  }
}

uint32_t ESP32TouchComponent::read_touch_value(touch_pad_t pad) const {
  // Unlike ESP32 v1, touch reads on ESP32-S2/S3 v2 are non-blocking operations.
  // The hardware continuously samples in the background and we can read the
  // latest value at any time without waiting.
  uint32_t value = 0;
  if (this->filter_configured_()) {
    // Read filtered/smoothed value when filter is enabled
    touch_pad_filter_read_smooth(pad, &value);
  } else {
    // Read raw value when filter is not configured
    touch_pad_read_raw_data(pad, &value);
  }
  return value;
}

}  // namespace esp32_touch
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
