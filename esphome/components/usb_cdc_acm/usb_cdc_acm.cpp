#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_cdc_acm.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
namespace esphome::usb_cdc_acm {

static const char *const TAG = "usb_cdc_acm";

// Global component instance for managing USB device
USBCDCACMComponent *global_usb_cdc_component = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

//==============================================================================
// USBCDCACMInstance Implementation
//==============================================================================

void USBCDCACMInstance::queue_line_state_event(bool dtr, bool rts) {
  // Allocate event from pool
  CDCEvent *event = this->event_pool_.allocate();
  if (event == nullptr) {
    ESP_LOGW(TAG, "Event pool exhausted, line state event dropped (itf=%d)", this->itf_);
    return;
  }

  event->type = CDC_EVENT_LINE_STATE_CHANGED;
  event->data.line_state.dtr = dtr;
  event->data.line_state.rts = rts;

  if (!this->event_queue_.push(event)) {
    ESP_LOGW(TAG, "Event queue full, line state event dropped (itf=%d)", this->itf_);
    // Return event to pool since we couldn't queue it
    this->event_pool_.release(event);
  } else {
    // Wake main loop immediately to process event
#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
    App.wake_loop_threadsafe();
#endif
  }
}

void USBCDCACMInstance::queue_line_coding_event(uint32_t bit_rate, uint8_t stop_bits, uint8_t parity,
                                                uint8_t data_bits) {
  // Allocate event from pool
  CDCEvent *event = this->event_pool_.allocate();
  if (event == nullptr) {
    ESP_LOGW(TAG, "Event pool exhausted, line coding event dropped (itf=%d)", this->itf_);
    return;
  }

  event->type = CDC_EVENT_LINE_CODING_CHANGED;
  event->data.line_coding.bit_rate = bit_rate;
  event->data.line_coding.stop_bits = stop_bits;
  event->data.line_coding.parity = parity;
  event->data.line_coding.data_bits = data_bits;

  if (!this->event_queue_.push(event)) {
    ESP_LOGW(TAG, "Event queue full, line coding event dropped (itf=%d)", this->itf_);
    // Return event to pool since we couldn't queue it
    this->event_pool_.release(event);
  } else {
    // Wake main loop immediately to process event
#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
    App.wake_loop_threadsafe();
#endif
  }
}

void USBCDCACMInstance::process_events_() {
  // Process all pending events from the queue
  CDCEvent *event;
  while ((event = this->event_queue_.pop()) != nullptr) {
    switch (event->type) {
      case CDC_EVENT_LINE_STATE_CHANGED: {
        bool dtr = event->data.line_state.dtr;
        bool rts = event->data.line_state.rts;

        // Invoke user callback in main loop context
        if (this->line_state_callback_ != nullptr) {
          this->line_state_callback_(dtr, rts);
        }
        break;
      }
      case CDC_EVENT_LINE_CODING_CHANGED: {
        uint32_t bit_rate = event->data.line_coding.bit_rate;
        uint8_t stop_bits = event->data.line_coding.stop_bits;
        uint8_t parity = event->data.line_coding.parity;
        uint8_t data_bits = event->data.line_coding.data_bits;

        // Update UART configuration based on CDC line coding
        this->baud_rate_ = bit_rate;
        this->data_bits_ = data_bits;

        // Convert CDC stop bits to UART stop bits format
        // CDC: 0=1 stop bit, 1=1.5 stop bits, 2=2 stop bits
        this->stop_bits_ = (stop_bits == 0) ? 1 : (stop_bits == 1) ? 1 : 2;

        // Convert CDC parity to UART parity format
        // CDC: 0=None, 1=Odd, 2=Even, 3=Mark, 4=Space
        switch (parity) {
          case 0:
            this->parity_ = uart::UART_CONFIG_PARITY_NONE;
            break;
          case 1:
            this->parity_ = uart::UART_CONFIG_PARITY_ODD;
            break;
          case 2:
            this->parity_ = uart::UART_CONFIG_PARITY_EVEN;
            break;
          default:
            // Mark and Space parity are not commonly supported, default to None
            this->parity_ = uart::UART_CONFIG_PARITY_NONE;
            break;
        }

        // Invoke user callback in main loop context
        if (this->line_coding_callback_ != nullptr) {
          this->line_coding_callback_(bit_rate, stop_bits, parity, data_bits);
        }
        break;
      }
    }
    // Return event to pool for reuse
    this->event_pool_.release(event);
  }
}

//==============================================================================
// USBCDCACMComponent Implementation
//==============================================================================

USBCDCACMComponent::USBCDCACMComponent() { global_usb_cdc_component = this; }

void USBCDCACMComponent::setup() {
  // Setup all registered interfaces
  for (auto *interface : this->interfaces_) {
    if (interface != nullptr) {
      interface->setup();
    }
  }
}

void USBCDCACMComponent::loop() {
  // Call loop() on all registered interfaces to process events
  for (auto *interface : this->interfaces_) {
    if (interface != nullptr) {
      interface->loop();
    }
  }
}

void USBCDCACMComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "USB CDC-ACM:\n"
                "  Number of Interfaces: %d",
                ESPHOME_MAX_USB_CDC_INSTANCES);
  for (uint8_t i = 0; i < ESPHOME_MAX_USB_CDC_INSTANCES; ++i) {
    if (this->interfaces_[i] != nullptr) {
      this->interfaces_[i]->dump_config();
    } else {
      ESP_LOGCONFIG(TAG, "  Interface %u is disabled", i);
    }
  }
}

void USBCDCACMComponent::add_interface(USBCDCACMInstance *interface) {
  uint8_t itf_num = static_cast<uint8_t>(interface->get_itf());
  if (itf_num < ESPHOME_MAX_USB_CDC_INSTANCES) {
    this->interfaces_[itf_num] = interface;
  } else {
    ESP_LOGE(TAG, "Interface number must be less than %u", ESPHOME_MAX_USB_CDC_INSTANCES);
  }
}

USBCDCACMInstance *USBCDCACMComponent::get_interface_by_number(uint8_t itf) {
  for (auto *interface : this->interfaces_) {
    if ((interface != nullptr) && (interface->get_itf() == itf)) {
      return interface;
    }
  }
  return nullptr;
}

}  // namespace esphome::usb_cdc_acm
#endif
