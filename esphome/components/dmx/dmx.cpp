
#include "dmx.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <utility>

#if __has_include(<rdm/responder.h>)
#include "dmx/include/service.h"
#include "rdm/responder/include/utils.h"
#endif

#ifdef USE_ESP32
#include "driver/uart.h"
#endif

namespace esphome::dmx {

ESPHOME_LOG_TAG(TAG, "dmx");

// Manufacturer-specific PID range for RX sensor channels.
// RDM channel index 1 = PID 0x8001, index 2 = PID 0x8002, etc.
static constexpr rdm_pid_t RDM_CHANNEL_PID_BASE = 0x8001;

#if __has_include(<rdm/responder.h>)

// This handler is called for SET_COMMAND.
// GET uses the library's simple response handler.
static size_t dmx_dynamic_channel_set_handler(dmx_port_t dmx_num, const rdm_parameter_definition_t *,
                                              const rdm_header_t *header) {
  if (header->pdl != sizeof(uint16_t)) {
    return rdm_write_nack_reason(dmx_num, header, RDM_NR_FORMAT_ERROR);
  }

  uint16_t address = 0;

  const size_t read_size = rdm_read_pd(dmx_num, "w", &address, header->pdl);

  if (read_size != sizeof(address)) {
    return rdm_write_nack_reason(dmx_num, header, RDM_NR_FORMAT_ERROR);
  }

  if (address < 1 || address > 512) {
    return rdm_write_nack_reason(dmx_num, header, RDM_NR_DATA_OUT_OF_RANGE);
  }

  if (dmx_parameter_set(dmx_num, header->sub_device, header->pid, &address, sizeof(address)) != sizeof(address)) {
    return rdm_write_nack_reason(dmx_num, header, RDM_NR_HARDWARE_FAULT);
  }

  return rdm_write_ack(dmx_num, header, nullptr, nullptr, 0);
}

static rdm_parameter_definition_t make_channel_definition(RdmChannelType type, const char *description) {
  rdm_parameter_definition_t definition{};

  definition.pid_cc = type == RdmChannelType::DYNAMIC ? RDM_CC_GET_SET : RDM_CC_GET;

  definition.ds = RDM_DS_UNSIGNED_WORD;

  definition.get = {
      .request = {.format = nullptr},
      .response = {.format = "w"},
      .handler = rdm_simple_response_handler,
  };

  if (type == RdmChannelType::DYNAMIC) {
    definition.set = {
        .request = {.format = "w"},
        .response = {.format = nullptr},
        .handler = dmx_dynamic_channel_set_handler,
    };
  } else {
    definition.set = {
        .request = {.format = nullptr},
        .response = {.format = nullptr},
        .handler = nullptr,
    };
  }

  definition.pdl_size = sizeof(uint16_t);
  definition.max_value = 512;
  definition.min_value = 1;
  definition.default_value = 1;
  definition.units = RDM_UNITS_NONE;
  definition.prefix = RDM_PREFIX_NONE;
  definition.description = description;

  return definition;
}

#endif  // __has_include(<rdm/responder.h>)

void DMXComponent::setup() {
  this->dmx_port_ = static_cast<dmx_port_t>(this->uart_num_);

  this->rx_data_[0] = 0;
  this->tx_data_[0] = 0;

#ifdef USE_ESP32
  if (uart_is_driver_installed(static_cast<uart_port_t>(this->uart_num_))) {
    ESP_LOGE(TAG, "UART%d is already in use. Check logger and UART configuration.", this->uart_num_);
    this->mark_failed();
    return;
  }
#endif

  dmx_config_t config = DMX_CONFIG_DEFAULT;

  if constexpr (ESPHOME_DMX_HAS_RDM) {
    if (this->mode_ == DMXMode::DMX_MODE_RX && this->rdm_enabled_) {
      // Built-in RDM parameters plus one parameter per RX sensor channel.
      config.root_device_parameter_count = static_cast<uint32_t>(32 + this->rdm_channels_.size());

      // This component acts as a DMX interface.
      config.product_category = RDM_PRODUCT_CATEGORY_DATA;
    }
  }

  if (!dmx_driver_install(this->dmx_port_, &config, nullptr, 0)) {
    ESP_LOGE(TAG, "Failed to install DMX driver on UART%d", this->uart_num_);
    this->mark_failed();
    return;
  }

  if (!dmx_set_pin(this->dmx_port_, this->tx_pin_, this->rx_pin_, this->rts_pin_)) {
    ESP_LOGE(TAG, "Failed to configure DMX pins on UART%d", this->uart_num_);
    dmx_driver_delete(this->dmx_port_);
    this->mark_failed();
    return;
  }

  if constexpr (ESPHOME_DMX_HAS_RDM) {
    if (this->mode_ == DMXMode::DMX_MODE_RX && this->rdm_enabled_) {
      this->setup_rdm_();
    }
  }

  const bool rdm_active = ESPHOME_DMX_HAS_RDM && this->mode_ == DMXMode::DMX_MODE_RX && this->rdm_enabled_;

  ESP_LOGI(TAG, "DMX started on UART%d (TX=%d RX=%d RTS=%d) RDM=%s", this->uart_num_, this->tx_pin_, this->rx_pin_,
           this->rts_pin_, rdm_active ? LOG_STR_LITERAL("on") : LOG_STR_LITERAL("off"));
}

void DMXComponent::setup_rdm_() {
  if constexpr (ESPHOME_DMX_HAS_RDM) {
#if __has_include(<rdm/responder.h>)

    const size_t count = this->rdm_channels_.size();

    if (count > 104) {
      ESP_LOGW(TAG,
               "RDM: %u channels registered; only the first 104 custom parameters "
               "fit in SUPPORTED_PARAMETERS",
               static_cast<unsigned>(count));
    }

    // Reserve memory before storing pointers to descriptions and definitions.
    this->rdm_descriptions_.reserve(count);
    this->rdm_definitions_.reserve(count);

    // Build all descriptions first so their c_str() pointers remain stable.
    for (size_t i = 0; i < count; i++) {
      const auto &channel = this->rdm_channels_[i];

      std::string description;

      if (channel.sensor_entity != nullptr) {
        char name_buf[ENTITY_NAME_BUF_SIZE];
        const StringRef name = channel.sensor_entity->get_name_to(name_buf);
        description = "ESPHome RX sensor: ";
        description.append(name.c_str(), name.size());
      } else {
        char channel_index[UINT32_MAX_STR_SIZE];
        uint32_to_str(channel_index, static_cast<uint32_t>(i + 1));

        description = std::string("ESPHome RX channel ") + channel_index;
      }

      // RDM PARAMETER_DESCRIPTION supports up to 32 ASCII characters.
      if (description.size() > 32) {
        description.resize(32);
      }

      this->rdm_descriptions_.push_back(std::move(description));
    }

    // Create one parameter definition for every registered RX channel.
    for (size_t i = 0; i < count; i++) {
      const auto &channel = this->rdm_channels_[i];

      this->rdm_definitions_.push_back(make_channel_definition(channel.type, this->rdm_descriptions_[i].c_str()));
    }

    // Register all channels in their original sensor registration order.
    for (size_t i = 0; i < count; i++) {
      auto &channel = this->rdm_channels_[i];

      if (channel.dmx_address < 1 || channel.dmx_address > 512) {
        channel.dmx_address = 1;
      }

      const rdm_pid_t pid = static_cast<rdm_pid_t>(RDM_CHANNEL_PID_BASE + i);

      const int parameter_type =
          channel.type == RdmChannelType::FIXED ? DMX_PARAMETER_TYPE_STATIC : DMX_PARAMETER_TYPE_DYNAMIC;

      if (!dmx_parameter_add(this->dmx_port_, RDM_SUB_DEVICE_ROOT, pid, parameter_type, &channel.dmx_address,
                             sizeof(channel.dmx_address))) {
        ESP_LOGW(TAG, "RDM: failed to add channel %u (PID 0x%04X)", static_cast<unsigned>(i + 1),
                 static_cast<unsigned>(pid));
        continue;
      }

      if (!rdm_definition_set(this->dmx_port_, RDM_SUB_DEVICE_ROOT, pid, &this->rdm_definitions_[i])) {
        ESP_LOGW(TAG, "RDM: failed to define channel %u (PID 0x%04X)", static_cast<unsigned>(i + 1),
                 static_cast<unsigned>(pid));
        continue;
      }

      ESP_LOGI(TAG, "RDM: channel %u registered, PID 0x%04X, DMX address %u, type %s", static_cast<unsigned>(i + 1),
               static_cast<unsigned>(pid), channel.dmx_address,
               channel.type == RdmChannelType::DYNAMIC ? LOG_STR_LITERAL("dynamic") : LOG_STR_LITERAL("fixed"));
    }

    // Synchronize any Number states that became available during setup.
    this->sync_numbers_to_rdm_();

    if (count == 0) {
      ESP_LOGD(TAG, "RDM enabled; no RX sensor channels registered");
    }

#endif
  } else {
    ESP_LOGW(TAG, "RDM requested but rdm/responder.h not available in esp_dmx build");
  }
}

void DMXComponent::sync_rdm_to_numbers_() {
  if constexpr (ESPHOME_DMX_HAS_RDM) {
#if __has_include(<rdm/responder.h>)

    for (size_t i = 0; i < this->rdm_channels_.size(); i++) {
      auto &channel = this->rdm_channels_[i];

      if (channel.type != RdmChannelType::DYNAMIC || channel.number_entity == nullptr) {
        continue;
      }

      const rdm_pid_t pid = static_cast<rdm_pid_t>(RDM_CHANNEL_PID_BASE + i);

      uint16_t address = 0;

      if (dmx_parameter_copy(this->dmx_port_, RDM_SUB_DEVICE_ROOT, pid, &address, sizeof(address)) != sizeof(address)) {
        continue;
      }

      if (address < 1 || address > 512 || address == channel.dmx_address) {
        continue;
      }

      // Send the RDM value through the existing ESPHome Number control path.
      auto call = channel.number_entity->make_call();
      call.set_value(static_cast<float>(address));
      call.perform();

      // Update our mirror from the Number's resulting state.
      const float actual = channel.number_entity->state;

      if (std::isfinite(actual) && actual >= 1.0f && actual <= 512.0f && std::floor(actual) == actual) {
        channel.dmx_address = static_cast<uint16_t>(actual);

        ESP_LOGI(TAG, "RDM → Number '%s' = %u", LOG_STR_ARG(channel.number_entity->get_log_name()),
                 channel.dmx_address);
      }
    }

#endif
  }
}

void DMXComponent::sync_numbers_to_rdm_() {
  if constexpr (ESPHOME_DMX_HAS_RDM) {
#if __has_include(<rdm/responder.h>)

    for (size_t i = 0; i < this->rdm_channels_.size(); i++) {
      auto &channel = this->rdm_channels_[i];

      if (channel.type != RdmChannelType::DYNAMIC || channel.number_entity == nullptr ||
          !channel.number_entity->has_state()) {
        continue;
      }

      const float state = channel.number_entity->state;

      if (!std::isfinite(state) || state < 1.0f || state > 512.0f || std::floor(state) != state) {
        continue;
      }

      const uint16_t address = static_cast<uint16_t>(state);

      if (address == channel.dmx_address) {
        continue;
      }

      const rdm_pid_t pid = static_cast<rdm_pid_t>(RDM_CHANNEL_PID_BASE + i);

      if (dmx_parameter_set(this->dmx_port_, RDM_SUB_DEVICE_ROOT, pid, &address, sizeof(address)) == sizeof(address)) {
        channel.dmx_address = address;

        ESP_LOGD(TAG, "Number '%s' → RDM channel %u = %u", LOG_STR_ARG(channel.number_entity->get_log_name()),
                 static_cast<unsigned>(i + 1), address);
      }
    }

#endif
  }
}

void DMXComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();

  // RX mode: receive DMX and handle RDM.
  if (this->mode_ == DMXMode::DMX_MODE_RX) {
    dmx_packet_t packet{};

    const size_t received_size = dmx_receive(this->dmx_port_, &packet, 1);

    if (received_size > 0) {
      // Handle valid RDM packets immediately.
      if (packet.is_rdm) {
        if constexpr (ESPHOME_DMX_HAS_RDM) {
          if (this->rdm_enabled_) {
            rdm_send_response(this->dmx_port_);
            this->sync_rdm_to_numbers_();
          }
        }
        return;
      }

      // Handle normal DMX packets, including shorter-than-expected frames.
      if ((packet.err == DMX_OK || packet.err == DMX_ERR_NOT_ENOUGH_SLOTS) && !packet.is_rdm && packet.sc == 0x00) {
        const size_t packet_size = packet.size;

        if (packet_size == 0 || packet_size > DMX_PACKET_SIZE) {
          ESP_LOGW(TAG, "Ignoring invalid DMX packet size (%u bytes)", static_cast<unsigned>(packet_size));
        } else {
          const size_t bytes_read = dmx_read(this->dmx_port_, this->rx_data_, packet_size);

          if (bytes_read == packet_size) {
            this->publish_inputs_(packet_size);
          } else {
            ESP_LOGW(TAG, "Incomplete DMX read: expected %u bytes, received %u", static_cast<unsigned>(packet_size),
                     static_cast<unsigned>(bytes_read));
          }
        }
      } else if (packet.err != DMX_OK && packet.err != DMX_ERR_NOT_ENOUGH_SLOTS) {
        ESP_LOGW(TAG, "DMX error: %d", packet.err);
      }
    }

    // Synchronize Home Assistant Number changes to RDM parameters.
    if constexpr (ESPHOME_DMX_HAS_RDM) {
      if (this->rdm_enabled_) {
        this->sync_numbers_to_rdm_();
      }
    }

    return;
  }

  // TX mode: ESPHome is the DMX controller.
  if (this->mode_ == DMXMode::DMX_MODE_TX && this->tx_enabled_) {
    if (this->tx_dirty_ || (now - this->last_tx_ms_ >= this->tx_interval_ms_)) {
      this->send_universe_();
    }
  }
}

void DMXComponent::publish_inputs_(size_t received_size) {
  // Fixed RX channels.
  for (auto &cs : this->static_sensors_) {
    if (cs.channel < 1 || cs.channel > 512) {
      continue;
    }

    if (cs.channel >= received_size) {
      continue;
    }

    const float value = static_cast<float>(this->rx_data_[cs.channel]);

    if (!cs.sensor->has_state() || cs.sensor->get_raw_state() != value) {
      cs.sensor->publish_state(value);
    }
  }

  // Dynamic RX channels.
  for (auto &ds : this->dynamic_sensors_) {
    if (ds.channel_number == nullptr || !ds.channel_number->has_state()) {
      continue;
    }

    const float channel_value = ds.channel_number->state;

    if (!std::isfinite(channel_value) || channel_value < 1.0f || channel_value > 512.0f) {
      continue;
    }

    const uint16_t channel = static_cast<uint16_t>(channel_value);

    if (static_cast<float>(channel) != channel_value) {
      continue;
    }

    if (channel >= received_size) {
      continue;
    }

    const float value = static_cast<float>(this->rx_data_[channel]);

    if (!ds.sensor->has_state() || ds.sensor->get_raw_state() != value) {
      ds.sensor->publish_state(value);
    }
  }
}

void DMXComponent::send_universe_() {
  // Don't overwrite the driver buffer while a frame is still going out.
  if (!dmx_wait_sent(this->dmx_port_, 1)) {
    // Still busy — keep tx_dirty_ so we retry next loop.
    return;
  }

  const size_t written = dmx_write(this->dmx_port_, this->tx_data_, DMX_PACKET_SIZE);

  if (written != DMX_PACKET_SIZE) {
    ESP_LOGW(TAG, "DMX write incomplete: %u / %u", static_cast<unsigned>(written),
             static_cast<unsigned>(DMX_PACKET_SIZE));
    return;
  }

  const size_t sent = dmx_send_num(this->dmx_port_, DMX_PACKET_SIZE);

  if (sent != DMX_PACKET_SIZE) {
    ESP_LOGW(TAG, "DMX send incomplete: %u / %u", static_cast<unsigned>(sent), static_cast<unsigned>(DMX_PACKET_SIZE));
    return;
  }

  this->tx_dirty_ = false;
  this->last_tx_ms_ = App.get_loop_component_start_time();
}

uint8_t DMXComponent::get_rx_channel(uint16_t channel) const {
  if (channel < 1 || channel > 512) {
    return 0;
  }

  return this->rx_data_[channel];
}

uint8_t DMXComponent::get_tx_channel(uint16_t channel) const {
  if (channel < 1 || channel > 512) {
    return 0;
  }

  return this->tx_data_[channel];
}

void DMXComponent::set_slot(uint16_t channel, uint8_t value) { this->set_slots(channel, &value, 1); }

void DMXComponent::set_slots(uint16_t first_channel, const uint8_t *values, size_t count) {
  if (values == nullptr || count == 0 || first_channel < 1 || first_channel > 512) {
    return;
  }

  // Avoid writing beyond channel 512.
  // Invalid ranges are ignored as a whole.
  if (count > (513U - static_cast<size_t>(first_channel))) {
    return;
  }

  bool changed = false;

  for (size_t i = 0; i < count; i++) {
    const size_t index = static_cast<size_t>(first_channel) + i;

    if (this->tx_data_[index] != values[i]) {
      this->tx_data_[index] = values[i];
      changed = true;
    }
  }

  if (changed) {
    this->tx_dirty_ = true;
  }
}

void DMXComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DMX Hub:");
  ESP_LOGCONFIG(TAG, "  UART: %d", this->uart_num_);
  ESP_LOGCONFIG(TAG, "  Mode: %u", static_cast<unsigned>(this->mode_));
  ESP_LOGCONFIG(TAG, "  TX Pin: GPIO%d", this->tx_pin_);
  ESP_LOGCONFIG(TAG, "  RX Pin: GPIO%d", this->rx_pin_);

  if (this->rts_pin_ >= 0) {
    ESP_LOGCONFIG(TAG, "  RTS Pin: GPIO%d", this->rts_pin_);
  }

  ESP_LOGCONFIG(TAG, "  TX interval: %u ms", static_cast<unsigned>(this->tx_interval_ms_));

  const bool rdm_active = ESPHOME_DMX_HAS_RDM && this->mode_ == DMXMode::DMX_MODE_RX && this->rdm_enabled_;

  ESP_LOGCONFIG(TAG, "  RDM: %s", rdm_active ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"));

  if (rdm_active) {
    ESP_LOGCONFIG(TAG, "  RDM RX channels: %u", static_cast<unsigned>(this->rdm_channels_.size()));

    for (size_t i = 0; i < this->rdm_channels_.size(); i++) {
      const auto &channel = this->rdm_channels_[i];

      if (channel.type == RdmChannelType::DYNAMIC && channel.number_entity != nullptr) {
        ESP_LOGCONFIG(TAG, "    [%u] '%s' → DMX address %u, dynamic, PID 0x%04X", static_cast<unsigned>(i + 1),
                      LOG_STR_ARG(channel.number_entity->get_log_name()), channel.dmx_address,
                      static_cast<unsigned>(RDM_CHANNEL_PID_BASE + i));
      } else {
        ESP_LOGCONFIG(TAG, "    [%u] DMX address %u, fixed, PID 0x%04X", static_cast<unsigned>(i + 1),
                      channel.dmx_address, static_cast<unsigned>(RDM_CHANNEL_PID_BASE + i));
      }
    }
  }
}

light::LightTraits DMXLightOutput::get_traits() {
  auto traits = light::LightTraits();

  switch (this->light_type_) {
    case DMXLightType::DMX_LIGHT_TYPE_DIMMER:
      traits.set_supported_color_modes({light::ColorMode::BRIGHTNESS});
      break;

    case DMXLightType::DMX_LIGHT_TYPE_RGB:
      traits.set_supported_color_modes({light::ColorMode::RGB});
      break;

    case DMXLightType::DMX_LIGHT_TYPE_RGBW:
      traits.set_supported_color_modes({light::ColorMode::RGB_WHITE});
      break;
  }

  return traits;
}

void DMXLightOutput::write_state(light::LightState *state) {
  if (this->parent_ == nullptr || state == nullptr) {
    return;
  }

  uint8_t values[4]{};
  size_t count = 0;

  switch (this->light_type_) {
    case DMXLightType::DMX_LIGHT_TYPE_DIMMER: {
      float brightness;
      state->current_values_as_brightness(&brightness);

      values[0] = static_cast<uint8_t>(std::lroundf(brightness * 255.0f));

      count = 1;
      break;
    }

    case DMXLightType::DMX_LIGHT_TYPE_RGB: {
      float red, green, blue;
      state->current_values_as_rgb(&red, &green, &blue);

      values[0] = static_cast<uint8_t>(std::lroundf(red * 255.0f));
      values[1] = static_cast<uint8_t>(std::lroundf(green * 255.0f));
      values[2] = static_cast<uint8_t>(std::lroundf(blue * 255.0f));

      count = 3;
      break;
    }

    case DMXLightType::DMX_LIGHT_TYPE_RGBW: {
      float red, green, blue, white;
      state->current_values_as_rgbw(&red, &green, &blue, &white);

      values[0] = static_cast<uint8_t>(std::lroundf(red * 255.0f));
      values[1] = static_cast<uint8_t>(std::lroundf(green * 255.0f));
      values[2] = static_cast<uint8_t>(std::lroundf(blue * 255.0f));
      values[3] = static_cast<uint8_t>(std::lroundf(white * 255.0f));

      count = 4;
      break;
    }
  }

  this->parent_->enable_tx();
  this->parent_->set_slots(this->first_channel_, values, count);
}

}  // namespace esphome::dmx
