#include "dmx.h"
#include "esphome/core/log.h"

#if ESPHOME_DMX_HAS_RDM
#include "dmx/include/service.h"
#include "rdm/responder/include/utils.h"
#endif

#ifdef USE_ESP32
#include "driver/uart.h"
#endif

namespace esphome::dmx {

ESPHOME_LOG_TAG(TAG, "dmx");

// Manufacturer-specific PID base for extra channel bindings (root uses standard PID)
static constexpr rdm_pid_t RDM_CUSTOM_PID_BASE = 0x8001;
// Fixed channel sensors use a separate manufacturer-PID range.
static constexpr rdm_pid_t RDM_STATIC_PID_BASE = 0x8800;

#if ESPHOME_DMX_HAS_RDM
// The definition objects must outlive the DMX driver; the driver stores pointers.
static const rdm_parameter_definition_t RDM_FIXED_CHANNEL_DEFINITION = {
    .pid_cc = RDM_CC_GET,
    .ds = RDM_DS_UNSIGNED_WORD,
    .get = {.request = {.format = nullptr}, .response = {.format = "w"}, .handler = rdm_simple_response_handler},
    .set = {.request = {.format = nullptr}, .response = {.format = nullptr}, .handler = nullptr},
    .pdl_size = sizeof(uint16_t),
    .max_value = 512,
    .min_value = 1,
    .default_value = 1,
    .units = RDM_UNITS_NONE,
    .prefix = RDM_PREFIX_NONE,
    .description = "Fixed ESPHome DMX channel",
};

// This handler is called only for SET_COMMAND. GET uses the library's simple
// response handler, which reads the parameter value registered below.
static size_t dmx_dynamic_channel_set_handler(dmx_port_t dmx_num, const rdm_parameter_definition_t *,
                                              const rdm_header_t *header) {
  if (header->pdl != sizeof(uint16_t))
    return rdm_write_nack_reason(dmx_num, header, RDM_NR_FORMAT_ERROR);

  uint16_t address = 0;
  const size_t read_size = rdm_read_pd(dmx_num, "w", &address, header->pdl);
  if (read_size != sizeof(address))
    return rdm_write_nack_reason(dmx_num, header, RDM_NR_FORMAT_ERROR);
  if (address < 1 || address > 512)
    return rdm_write_nack_reason(dmx_num, header, RDM_NR_DATA_OUT_OF_RANGE);

  if (dmx_parameter_set(dmx_num, header->sub_device, header->pid, &address, sizeof(address)) != sizeof(address))
    return rdm_write_nack_reason(dmx_num, header, RDM_NR_HARDWARE_FAULT);

  return rdm_write_ack(dmx_num, header, nullptr, nullptr, 0);
}

static const rdm_parameter_definition_t RDM_DYNAMIC_CHANNEL_DEFINITION = {
    .pid_cc = RDM_CC_GET_SET,
    .ds = RDM_DS_UNSIGNED_WORD,
    .get = {.request = {.format = nullptr}, .response = {.format = "w"}, .handler = rdm_simple_response_handler},
    .set = {.request = {.format = "w"}, .response = {.format = nullptr}, .handler = dmx_dynamic_channel_set_handler},
    .pdl_size = sizeof(uint16_t),
    .max_value = 512,
    .min_value = 1,
    .default_value = 1,
    .units = RDM_UNITS_NONE,
    .prefix = RDM_PREFIX_NONE,
    .description = "Dynamic ESPHome DMX channel",
};
#endif

void DMXComponent::setup() {
  this->dmx_port_ = static_cast<dmx_port_t>(this->uart_num_);
  this->rx_data_[0] = 0;  // null start code
  this->tx_data_[0] = 0;  // null start code

#ifdef USE_ESP32
  if (uart_is_driver_installed(static_cast<uart_port_t>(this->uart_num_))) {
    ESP_LOGE(TAG,
             "UART%d is already in use. "
             "Check logger and UART configuration.",
             this->uart_num_);
    this->mark_failed();
    return;
  }
#endif

  dmx_config_t config = DMX_CONFIG_DEFAULT;
  if constexpr (ESPHOME_DMX_HAS_RDM) {
    // Built-in RDM PIDs plus one entry for each fixed and dynamic channel.
    config.root_device_parameter_count =
        static_cast<uint32_t>(32 + this->static_sensors_.size() + this->rdm_bindings_.size());
  }

  dmx_personality_t personalities[] = {{1, "ESPHome DMX"}};

  if (!dmx_driver_install(this->dmx_port_, &config, personalities, 1)) {
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

  if (this->rdm_enabled_) {
    this->setup_rdm_();
  }

  ESP_LOGI(TAG, "DMX started on UART%d (TX=%d RX=%d RTS=%d) RDM=%s", this->uart_num_, this->tx_pin_, this->rx_pin_,
           this->rts_pin_, this->rdm_enabled_ ? LOG_STR_LITERAL("on") : LOG_STR_LITERAL("off"));
}

void DMXComponent::register_rdm_channel(number::Number *channel_number) {
  if (channel_number == nullptr)
    return;

  // Avoid duplicates
  for (const auto &b : this->rdm_bindings_) {
    if (b.channel_number == channel_number)
      return;
  }

  RdmChannelBinding binding{};
  binding.channel_number = channel_number;
  binding.last_address = 1;
  if (channel_number->has_state()) {
    const float state = channel_number->state;
    if (state >= 1.0f && state <= 512.0f)
      binding.last_address = static_cast<uint16_t>(state);
  }

  // First binding uses standard root DMX_START_ADDRESS (custom_pid = 0)
  // Further bindings get manufacturer PIDs
  if (this->rdm_bindings_.empty()) {
    binding.custom_pid = 0;
  } else {
    binding.custom_pid = static_cast<rdm_pid_t>(RDM_CUSTOM_PID_BASE + (this->rdm_bindings_.size() - 1));
  }

  this->rdm_bindings_.push_back(binding);
}

void DMXComponent::setup_rdm_() {
  if constexpr (ESPHOME_DMX_HAS_RDM) {
    // Fixed channel sensors: expose their configured channel number as GET-only PIDs.
    // The parameter points directly at the stable channel field in static_sensors_.
    for (auto &cs : this->static_sensors_) {
      if (cs.channel < 1 || cs.channel > 512)
        continue;

      const rdm_pid_t pid = static_cast<rdm_pid_t>(RDM_STATIC_PID_BASE + cs.channel);
      if (!dmx_parameter_add(this->dmx_port_, RDM_SUB_DEVICE_ROOT, pid, DMX_PARAMETER_TYPE_STATIC,
                             &cs.channel, sizeof(cs.channel))) {
        ESP_LOGW(TAG, "RDM: failed to add fixed-channel PID 0x%04X", pid);
        continue;
      }
      if (!rdm_definition_set(this->dmx_port_, RDM_SUB_DEVICE_ROOT, pid, &RDM_FIXED_CHANNEL_DEFINITION)) {
        ESP_LOGW(TAG, "RDM: failed to define fixed-channel PID 0x%04X", pid);
        continue;
      }
      ESP_LOGD(TAG, "RDM: fixed channel %u registered as GET-only PID 0x%04X",
               static_cast<unsigned>(cs.channel), pid);
    }

    // The first dynamic Number uses the standard RDM DMX_START_ADDRESS PID.
    if (!this->rdm_bindings_.empty()) {
      auto &first = this->rdm_bindings_[0];
      if (!rdm_set_dmx_start_address(this->dmx_port_, first.last_address)) {
        ESP_LOGW(TAG, "RDM: failed to initialize DMX_START_ADDRESS to %u", first.last_address);
      }

      // Additional dynamic Numbers get unique manufacturer-specific GET/SET PIDs.
      for (size_t i = 1; i < this->rdm_bindings_.size(); i++) {
        auto &binding = this->rdm_bindings_[i];
        uint16_t initial_address = binding.last_address;

        if (!dmx_parameter_add(this->dmx_port_, RDM_SUB_DEVICE_ROOT, binding.custom_pid,
                               DMX_PARAMETER_TYPE_DYNAMIC, &initial_address,
                               sizeof(initial_address))) {
          ESP_LOGW(TAG, "RDM: failed to add dynamic PID 0x%04X for binding %u",
                   binding.custom_pid, static_cast<unsigned>(i));
          continue;
        }
        if (!rdm_definition_set(this->dmx_port_, RDM_SUB_DEVICE_ROOT, binding.custom_pid,
                                &RDM_DYNAMIC_CHANNEL_DEFINITION)) {
          ESP_LOGW(TAG, "RDM: failed to define dynamic PID 0x%04X", binding.custom_pid);
          continue;
        }

        ESP_LOGI(TAG, "RDM: dynamic PID 0x%04X registered (channel %u)",
                 binding.custom_pid, initial_address);
      }

      const char *first_name = first.channel_number ? first.channel_number->get_name().c_str() : "?";
      ESP_LOGI(TAG, "RDM: root DMX_START_ADDRESS bound to '%s' (addr %u), %u dynamic binding(s)",
               first_name, first.last_address, static_cast<unsigned>(this->rdm_bindings_.size()));
    } else {
      ESP_LOGD(TAG, "RDM enabled; no dynamic Number bindings registered");
    }
  } else {
    ESP_LOGW(TAG, "RDM requested but rdm/responder.h not available in esp_dmx build");
  }
}

void DMXComponent::sync_rdm_to_numbers_() {
  if constexpr (ESPHOME_DMX_HAS_RDM) {
    if (this->rdm_bindings_.empty())
      return;

    // Root standard start address → first Number
    {
      auto &b = this->rdm_bindings_[0];
      uint16_t addr = 0;
      if (rdm_get_dmx_start_address(this->dmx_port_, &addr) && addr >= 1 && addr <= 512 && addr != b.last_address) {
        b.last_address = addr;
        if (b.channel_number != nullptr) {
          b.channel_number->publish_state(static_cast<float>(addr));
          ESP_LOGI(TAG, "RDM → Number '%s' = %u", b.channel_number->get_name().c_str(), addr);
        }
      }
    }

    // Custom PIDs → remaining Numbers
    for (size_t i = 1; i < this->rdm_bindings_.size(); i++) {
      auto &b = this->rdm_bindings_[i];
      uint16_t addr = 0;
      if (dmx_parameter_copy(this->dmx_port_, RDM_SUB_DEVICE_ROOT, b.custom_pid, &addr, sizeof(addr)) != sizeof(addr))
        continue;
      if (addr < 1 || addr > 512 || addr == b.last_address)
        continue;
      b.last_address = addr;
      if (b.channel_number != nullptr) {
        b.channel_number->publish_state(static_cast<float>(addr));
        ESP_LOGI(TAG, "RDM PID 0x%04X → Number '%s' = %u", b.custom_pid, b.channel_number->get_name().c_str(), addr);
      }
    }
  }
}

void DMXComponent::sync_numbers_to_rdm_() {
  if constexpr (ESPHOME_DMX_HAS_RDM) {
    if (this->rdm_bindings_.empty())
      return;

    // First Number → root start address
    {
      auto &b = this->rdm_bindings_[0];
      if (b.channel_number != nullptr && b.channel_number->has_state()) {
        const float state = b.channel_number->state;
        if (state >= 1.0f && state <= 512.0f) {
          const uint16_t addr = static_cast<uint16_t>(state);
          if (addr != b.last_address) {
            if (rdm_set_dmx_start_address(this->dmx_port_, addr)) {
              b.last_address = addr;
              ESP_LOGD(TAG, "Number '%s' → RDM start address %u", b.channel_number->get_name().c_str(), addr);
            }
          }
        }
      }
    }

    // Remaining Numbers → custom parameters
    for (size_t i = 1; i < this->rdm_bindings_.size(); i++) {
      auto &b = this->rdm_bindings_[i];
      if (b.channel_number == nullptr || !b.channel_number->has_state())
        continue;
      const float state = b.channel_number->state;
      if (state < 1.0f || state > 512.0f)
        continue;
      const uint16_t addr = static_cast<uint16_t>(state);
      if (addr == b.last_address)
        continue;
      if (dmx_parameter_set(this->dmx_port_, RDM_SUB_DEVICE_ROOT, b.custom_pid, &addr, sizeof(addr))) {
        b.last_address = addr;
        ESP_LOGD(TAG, "Number '%s' → RDM PID 0x%04X = %u", b.channel_number->get_name().c_str(), b.custom_pid, addr);
      }
    }
  }
}

void DMXComponent::loop() {
  const bool want_rx = (this->mode_ != DMXMode::DMX_MODE_TX);
  const bool want_tx = (this->mode_ != DMXMode::DMX_MODE_RX) && this->tx_enabled_;

  bool got_rx = false;
  if (want_rx) {
    dmx_packet_t packet{};
    const size_t received_size = dmx_receive(this->dmx_port_, &packet, 0);

    if (received_size > 0) {
      if constexpr (ESPHOME_DMX_HAS_RDM) {
        if (this->rdm_enabled_ && packet.is_rdm) {
          // Respond immediately; do not put logging or other slow work before this call.
          rdm_send_response(this->dmx_port_);
          this->sync_rdm_to_numbers_();
          this->last_rx_ms_ = millis();
          got_rx = true;
        }
      }
      if (packet.err == DMX_OK && !packet.is_rdm && packet.sc == 0x00) {
        if (received_size > DMX_PACKET_SIZE) {
          ESP_LOGW(TAG, "Ignoring oversized DMX packet (%u bytes)", static_cast<unsigned>(received_size));
        } else {
          const size_t bytes_read = dmx_read(this->dmx_port_, this->rx_data_, received_size);
          if (bytes_read == received_size) {
            this->publish_inputs_();
            this->last_rx_ms_ = millis();
            got_rx = true;
          } else {
            ESP_LOGW(TAG, "Incomplete DMX read: expected %u bytes, received %u", static_cast<unsigned>(received_size),
                     static_cast<unsigned>(bytes_read));
          }
        }
      } else if (packet.err != DMX_OK) {
        ESP_LOGW(TAG, "DMX error: %d", packet.err);
      }
    }
  }

  // Periodically push Number changes (from HA) towards RDM
  if constexpr (ESPHOME_DMX_HAS_RDM) {
    if (this->rdm_enabled_ && !this->rdm_bindings_.empty()) {
      const uint32_t now_rdm = millis();
      if (now_rdm - this->last_rdm_push_ms_ >= 500) {
        this->last_rdm_push_ms_ = now_rdm;
        this->sync_numbers_to_rdm_();
      }
    }
  }

  if (!want_tx)
    return;

  const uint32_t now = millis();

  // Half-duplex: na RX én na TX een ontvangst-window vrijhouden
  if (this->mode_ == DMXMode::DMX_MODE_BIDIRECTIONAL) {
    if (got_rx)
      return;
    // Nog in post-RX guard?
    if ((now - this->last_rx_ms_) < this->rx_guard_ms_)
      return;
    // Nog in post-TX receive window? (voorkomt meteen weer TX’en)
    if ((now - this->last_tx_ms_) < this->rx_guard_ms_)
      return;
  }

  if (this->tx_dirty_ || (now - this->last_tx_ms_ >= this->tx_interval_ms_)) {
    this->send_universe_();
  }
}

void DMXComponent::publish_inputs_() {
  for (auto &cs : this->static_sensors_) {
    if (cs.channel < 1 || cs.channel > 512)
      continue;
    const float value = static_cast<float>(this->rx_data_[cs.channel]);
    if (!cs.sensor->has_state() || cs.sensor->get_raw_state() != value)
      cs.sensor->publish_state(value);
  }

  for (auto &ds : this->dynamic_sensors_) {
    if (ds.channel_number == nullptr || !ds.channel_number->has_state())
      continue;
    const float channel_value = ds.channel_number->state;
    if (!std::isfinite(channel_value) || channel_value < 1.0f || channel_value > 512.0f)
      continue;
    const uint16_t channel = static_cast<uint16_t>(channel_value);
    if (static_cast<float>(channel) != channel_value)
      continue;
    const float value = static_cast<float>(this->rx_data_[channel]);
    if (!ds.sensor->has_state() || ds.sensor->get_raw_state() != value)
      ds.sensor->publish_state(value);
  }
}

void DMXComponent::send_universe_() {
  // Don't overwrite the driver buffer while a frame is still going out
  if (!dmx_wait_sent(this->dmx_port_, 0)) {
    // Still busy — keep tx_dirty_ so we retry next loop
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
  this->last_tx_ms_ = millis();
}

uint8_t DMXComponent::get_rx_channel(uint16_t channel) const {
  if (channel < 1 || channel > 512)
    return 0;
  return this->rx_data_[channel];
}

uint8_t DMXComponent::get_tx_channel(uint16_t channel) const {
  if (channel < 1 || channel > 512)
    return 0;
  return this->tx_data_[channel];
}

void DMXComponent::set_slot(uint16_t channel, uint8_t value) { this->set_slots(channel, &value, 1); }

void DMXComponent::set_slots(uint16_t first_channel, const uint8_t *values, size_t count) {
  if (values == nullptr || count == 0 || first_channel < 1 || first_channel > 512)
    return;

  // Avoid writing beyond channel 512. Invalid ranges are ignored as a whole.
  if (count > (513U - static_cast<size_t>(first_channel)))
    return;

  bool changed = false;
  for (size_t i = 0; i < count; i++) {
    const size_t index = static_cast<size_t>(first_channel) + i;
    if (this->tx_data_[index] != values[i]) {
      this->tx_data_[index] = values[i];
      changed = true;
    }
  }

  if (changed)
    this->tx_dirty_ = true;
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
  ESP_LOGCONFIG(TAG, "  RX guard: %u ms", static_cast<unsigned>(this->rx_guard_ms_));
  ESP_LOGCONFIG(TAG, "  RDM: %s", this->rdm_enabled_ ? LOG_STR_LITERAL("enabled") : LOG_STR_LITERAL("disabled"));
  ESP_LOGCONFIG(TAG, "  RDM channel bindings: %u", static_cast<unsigned>(this->rdm_bindings_.size()));
  for (size_t i = 0; i < this->rdm_bindings_.size(); i++) {
    const auto &b = this->rdm_bindings_[i];
    const char *name = b.channel_number ? b.channel_number->get_name().c_str() : "?";
    if (b.custom_pid == 0) {
      ESP_LOGCONFIG(TAG, "    [%u] '%s' → root DMX_START_ADDRESS", static_cast<unsigned>(i), name);
    } else {
      ESP_LOGCONFIG(TAG, "    [%u] '%s' → custom PID 0x%04X", static_cast<unsigned>(i), name, b.custom_pid);
    }
  }
}

void DMXOutputNumber::control(float value) {
  if (this->parent_ == nullptr)
    return;
  if (value < 0.0f)
    value = 0.0f;
  if (value > 255.0f)
    value = 255.0f;
  const uint8_t slot = static_cast<uint8_t>(value);
  this->parent_->set_slot(this->channel_, slot);
  this->publish_state(slot);
}

void DMXOutputNumber::dump_config() {
  ESP_LOGCONFIG(TAG, "DMX Output:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
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
  if (this->parent_ == nullptr || state == nullptr)
    return;

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

} // namespace esphome::dmx
