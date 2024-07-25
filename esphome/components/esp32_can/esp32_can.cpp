#ifdef USE_ESP32
#include "esp32_can.h"
#include "esphome/core/log.h"

#include <driver/twai.h>

// WORKAROUND, because CAN_IO_UNUSED is just defined as (-1) in this version
// of the framework which does not work with -fpermissive
#undef CAN_IO_UNUSED
#define CAN_IO_UNUSED ((gpio_num_t) -1)

namespace esphome {
namespace esp32_can {

static const char *const TAG = "esp32_can";

static bool get_bitrate(canbus::CanSpeed bitrate, twai_timing_config_t *t_config) {
  switch (bitrate) {
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32C3) || \
    defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32H6)
    case canbus::CAN_1KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_1KBITS();
      return true;
    case canbus::CAN_5KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_5KBITS();
      return true;
    case canbus::CAN_10KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_10KBITS();
      return true;
    case canbus::CAN_12K5BPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_12_5KBITS();
      return true;
    case canbus::CAN_16KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_16KBITS();
      return true;
    case canbus::CAN_20KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_20KBITS();
      return true;
#endif
    case canbus::CAN_25KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_25KBITS();
      return true;
    case canbus::CAN_50KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_50KBITS();
      return true;
    case canbus::CAN_100KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_100KBITS();
      return true;
    case canbus::CAN_125KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_125KBITS();
      return true;
    case canbus::CAN_250KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_250KBITS();
      return true;
    case canbus::CAN_500KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_500KBITS();
      return true;
    case canbus::CAN_800KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_800KBITS();
      return true;
    case canbus::CAN_1000KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_1MBITS();
      return true;
    default:
      return false;
  }
}

bool ESP32Can::setup_internal() {
  static uint8_t next_controller_id = 0;
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t) this->tx_, (gpio_num_t) this->rx_, TWAI_MODE_NORMAL);
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  twai_timing_config_t t_config;

  if (!get_bitrate(this->bit_rate_, &t_config)) {
    // invalid bit rate
    this->mark_failed();
    return false;
  }

  if (next_controller_id >= SOC_TWAI_CONTROLLER_NUM) {
      ESP_LOGW(TAG, "Maximum number of CAN components created already.");
      this->mark_failed();
      return false;
  }

  // Install TWAI driver
  this->controller_id_ = next_controller_id;
  g_config.controller_id = next_controller_id++;
  if (twai_driver_install_v2(&g_config, &t_config, &f_config, &this->bus) != ESP_OK) {
    // Failed to install driver
    this->mark_failed();
    return false;
  }

  // Start TWAI driver
  if (twai_start_v2(this->bus) != ESP_OK) {
    // Failed to start driver
    this->mark_failed();
    return false;
  }
  return true;
}

canbus::Error ESP32Can::send_message(struct canbus::CanFrame *frame) {
  if (frame->can_data_length_code > canbus::CAN_MAX_DATA_LENGTH) {
    return canbus::ERROR_FAILTX;
  }

  uint32_t flags = TWAI_MSG_FLAG_NONE;
  if (frame->use_extended_id) {
    flags |= TWAI_MSG_FLAG_EXTD;
  }
  if (frame->remote_transmission_request) {
    flags |= TWAI_MSG_FLAG_RTR;
  }

  twai_message_t message = {
      .flags = flags,
      .identifier = frame->can_id,
      .data_length_code = frame->can_data_length_code,
  };
  if (!frame->remote_transmission_request) {
    memcpy(message.data, frame->data, frame->can_data_length_code);
  }

  if (twai_transmit_v2(this->bus, &message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    return canbus::ERROR_OK;
  } else {
    return canbus::ERROR_ALLTXBUSY;
  }
}

canbus::Error ESP32Can::read_message(struct canbus::CanFrame *frame) {
  twai_message_t message;

  if (twai_receive_v2(this->bus, &message, 0) != ESP_OK) {
    return canbus::ERROR_NOMSG;
  }

  frame->can_id = message.identifier;
  frame->use_extended_id = message.flags & TWAI_MSG_FLAG_EXTD;
  frame->remote_transmission_request = message.flags & TWAI_MSG_FLAG_RTR;
  frame->can_data_length_code = message.data_length_code;

  if (!frame->remote_transmission_request) {
    size_t dlc =
        message.data_length_code < canbus::CAN_MAX_DATA_LENGTH ? message.data_length_code : canbus::CAN_MAX_DATA_LENGTH;
    memcpy(frame->data, message.data, dlc);
  }

  return canbus::ERROR_OK;
}

}  // namespace esp32_can
}  // namespace esphome

#endif
