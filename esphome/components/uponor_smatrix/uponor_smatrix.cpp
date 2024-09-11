#include "uponor_smatrix.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uponor_smatrix {

static const char *const TAG = "uponor_smatrix";

void UponorSmatrixComponent::setup() {
#ifdef USE_TIME
  if (this->time_id_ != nullptr) {
    this->time_id_->add_on_time_sync_callback([this] { this->send_time(); });
  }
#endif
}

void UponorSmatrixComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Uponor Smatrix");
  ESP_LOGCONFIG(TAG, "  System address: 0x%04X", this->address_);
#ifdef USE_TIME
  if (this->time_id_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Time synchronization: YES");
    ESP_LOGCONFIG(TAG, "  Time master device address: 0x%04X", this->time_device_address_);
  }
#endif

  this->check_uart_settings(19200);

  if (!this->unknown_devices_.empty()) {
    ESP_LOGCONFIG(TAG, "  Detected unknown device addresses:");
    for (auto device_address : this->unknown_devices_) {
      ESP_LOGCONFIG(TAG, "    0x%04X", device_address);
    }
  }
}

void UponorSmatrixComponent::loop() {
  const uint32_t now = millis();

  // Discard stale data
  if (!this->rx_buffer_.empty() && (now - this->last_rx_ > 50)) {
    ESP_LOGD(TAG, "Discarding %d bytes of unparsed data", this->rx_buffer_.size());
    this->rx_buffer_.clear();
  }

  // Read incoming data
  while (this->available()) {
    // The controller polls devices every 10 seconds in some units or continuously in others with around 200 ms between
    // devices. Remember timestamps so we can send our own packets when the bus is expected to be silent.
    this->last_rx_ = now;

    uint8_t byte;
    this->read_byte(&byte);
    if (this->parse_byte_(byte)) {
      this->rx_buffer_.clear();
    }
  }

  // Send packets during bus silence
  if (this->rx_buffer_.empty() && (now - this->last_rx_ > 50) && (now - this->last_rx_ < 100) &&
      (now - this->last_tx_ > 200)) {
#ifdef USE_TIME
    // Only build time packet when bus is silent and queue is empty to make sure we can send it right away
    if (this->send_time_requested_ && this->tx_queue_.empty() && this->do_send_time_())
      this->send_time_requested_ = false;
#endif
    // Send the next packet in the queue
    if (!this->tx_queue_.empty()) {
      auto packet = std::move(this->tx_queue_.front());
      this->tx_queue_.pop();

      this->write_array(packet);
      this->flush();

      this->last_tx_ = now;
    }
  }
}

bool UponorSmatrixComponent::parse_byte_(uint8_t byte) {
  this->rx_buffer_.push_back(byte);
  const uint8_t *packet = this->rx_buffer_.data();
  size_t packet_len = this->rx_buffer_.size();

  if (packet_len < 7) {
    // Minimum packet size is 7 bytes, wait for more
    return false;
  }

  uint16_t system_address = encode_uint16(packet[0], packet[1]);
  uint16_t device_address = encode_uint16(packet[2], packet[3]);
  uint16_t crc = encode_uint16(packet[packet_len - 1], packet[packet_len - 2]);

  uint16_t computed_crc = crc16(packet, packet_len - 2);
  if (crc != computed_crc) {
    // CRC did not match, more data might be coming
    return false;
  }

  ESP_LOGV(TAG, "Received packet: sys=%04X, dev=%04X, data=%s, crc=%04X", system_address, device_address,
           format_hex(&packet[4], packet_len - 6).c_str(), crc);

  // Detect or check system address
  if (this->address_ == 0) {
    ESP_LOGI(TAG, "Using detected system address 0x%04X", system_address);
    this->address_ = system_address;
  } else if (this->address_ != system_address) {
    // This should never happen except if the system address was set or detected incorrectly, so warn the user.
    ESP_LOGW(TAG, "Received packet from unknown system address 0x%04X", system_address);
    return true;
  }

  // Handle packet
  size_t data_len = (packet_len - 6) / 3;
  if (data_len == 0) {
    if (packet[4] == UPONOR_ID_REQUEST)
      ESP_LOGVV(TAG, "Ignoring request packet for device 0x%04X", device_address);
    return true;
  }

  // Decode packet payload data for easy access
  UponorSmatrixData data[data_len];
  for (int i = 0; i < data_len; i++) {
    data[i].id = packet[(i * 3) + 4];
    data[i].value = encode_uint16(packet[(i * 3) + 5], packet[(i * 3) + 6]);
  }

#ifdef USE_TIME
  // Detect device that acts as time master if not set explicitely
  if (this->time_device_address_ == 0 && data_len >= 2) {
    // The first thermostat paired to the controller will act as the time master. Time can only be manually adjusted at
    // this first thermostat. To synchronize time, we need to know its address, so we search for packets coming from a
    // thermostat sending both room temperature and time information.
    bool found_temperature = false;
    bool found_time = false;
    for (int i = 0; i < data_len; i++) {
      if (data[i].id == UPONOR_ID_ROOM_TEMP)
        found_temperature = true;
      if (data[i].id == UPONOR_ID_DATETIME1)
        found_time = true;
      if (found_temperature && found_time) {
        ESP_LOGI(TAG, "Using detected time device address 0x%04X", device_address);
        this->time_device_address_ = device_address;
        break;
      }
    }
  }
#endif

  // Forward data to device components
  bool found = false;
  for (auto *device : this->devices_) {
    if (device->address_ == device_address) {
      found = true;
      device->on_device_data(data, data_len);
    }
  }

  // Log unknown device addresses
  if (!found && !this->unknown_devices_.count(device_address)) {
    ESP_LOGI(TAG, "Received packet for unknown device address 0x%04X ", device_address);
    this->unknown_devices_.insert(device_address);
  }

  // Return true to reset buffer
  return true;
}

bool UponorSmatrixComponent::send(uint16_t device_address, const UponorSmatrixData *data, size_t data_len) {
  if (this->address_ == 0 || device_address == 0 || data == nullptr || data_len == 0)
    return false;

  // Assemble packet for send queue. All fields are big-endian except for the little-endian checksum.
  std::vector<uint8_t> packet;
  packet.reserve(6 + 3 * data_len);

  packet.push_back(this->address_ >> 8);
  packet.push_back(this->address_ >> 0);
  packet.push_back(device_address >> 8);
  packet.push_back(device_address >> 0);

  for (int i = 0; i < data_len; i++) {
    packet.push_back(data[i].id);
    packet.push_back(data[i].value >> 8);
    packet.push_back(data[i].value >> 0);
  }

  auto crc = crc16(packet.data(), packet.size());
  packet.push_back(crc >> 0);
  packet.push_back(crc >> 8);

  this->tx_queue_.push(packet);
  return true;
}

#ifdef USE_TIME
bool UponorSmatrixComponent::do_send_time_() {
  if (this->time_device_address_ == 0 || this->time_id_ == nullptr)
    return false;

  ESPTime now = this->time_id_->now();
  if (!now.is_valid())
    return false;

  uint8_t year = now.year - 2000;
  uint8_t month = now.month;
  // ESPHome days are [1-7] starting with Sunday, Uponor days are [0-6] starting with Monday
  uint8_t day_of_week = (now.day_of_week == 1) ? 6 : (now.day_of_week - 2);
  uint8_t day_of_month = now.day_of_month;
  uint8_t hour = now.hour;
  uint8_t minute = now.minute;
  uint8_t second = now.second;

  uint16_t time1 = (year & 0x7F) << 7 | (month & 0x0F) << 3 | (day_of_week & 0x07);
  uint16_t time2 = (day_of_month & 0x1F) << 11 | (hour & 0x1F) << 6 | (minute & 0x3F);
  uint16_t time3 = second;

  ESP_LOGI(TAG, "Sending local time: %04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day_of_month, now.hour,
           now.minute, now.second);

  UponorSmatrixData data[] = {{UPONOR_ID_DATETIME1, time1}, {UPONOR_ID_DATETIME2, time2}, {UPONOR_ID_DATETIME3, time3}};
  return this->send(this->time_device_address_, data, sizeof(data) / sizeof(data[0]));
}
#endif

}  // namespace uponor_smatrix
}  // namespace esphome
