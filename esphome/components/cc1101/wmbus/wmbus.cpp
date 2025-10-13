#include "wmbus.h"
#include "version.h"

#include "meters.h"

#include "address.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"


#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif

#ifdef USE_ESP32
SET_LOOP_TASK_STACK_SIZE(32 * 1024);
#pragma message ( "Loop task stack increased." )
#endif
#ifdef USE_ESP8266
#error "ESP8266 not supported. Please use version 3.x: https://github.com/SzczepanLeon/esphome-components/issues/131"
#endif

namespace esphome {
namespace wmbus {

  static const char *TAG = "wmbus";

  void InfoComponent::setup() {
    return;
  }

  void WMBusComponent::setup() {
    this->high_freq_.start();
    if (this->led_pin_ != nullptr) {
      this->led_pin_->setup();
      this->led_pin_->digital_write(false);
      this->led_on_ = false;
    }
    if (!rf_mbus_.init(this->spi_conf_.mosi->get_pin(), this->spi_conf_.miso->get_pin(),
                       this->spi_conf_.clk->get_pin(),  this->spi_conf_.cs->get_pin(),
                       this->spi_conf_.gdo0->get_pin(), this->spi_conf_.gdo2->get_pin(),
                       this->frequency_, this->sync_mode_)) {
      this->mark_failed();
      ESP_LOGE(TAG, "RF chip initialization failed");
      return;
    }
#ifdef USE_WMBUS_MQTT
    this->mqtt_client_.setClient(this->tcp_client_);
    this->mqtt_client_.setServer(this->mqtt_->ip, this->mqtt_->port);
    this->mqtt_client_.setBufferSize(1000);
#endif
  }

  void WMBusComponent::loop() {
    this->led_handler();
    if (rf_mbus_.task()) {
      ESP_LOGVV(TAG, "Have data from RF ...");
      WMbusFrame mbus_data = rf_mbus_.get_frame();

      std::string telegram = format_hex_pretty(mbus_data.frame);
      telegram.erase(std::remove(telegram.begin(), telegram.end(), '.'), telegram.end());

      this->frame_timestamp_ = this->time_->timestamp_now();
      send_to_clients(mbus_data);
      Telegram t;
      if (t.parseHeader(mbus_data.frame) && t.addresses.empty()) {
        ESP_LOGE(TAG, "Address is empty! T: %s", telegram.c_str());
      }
      else {
        uint32_t meter_id = (uint32_t)strtoul(t.addresses[0].id.c_str(), nullptr, 16);
        bool meter_in_config = (this->wmbus_listeners_.count(meter_id) == 1) ? true : false;
        
        if (this->log_all_ || meter_in_config) { //No need to do sth if logging is disabled and meter is not configured

          auto detected_drv_info      = pickMeterDriver(&t);
          std::string detected_driver = (detected_drv_info.name().str().empty() ? "" : detected_drv_info.name().str().c_str());

          //If the driver was explicitly stated in meter config, use that driver instead on detected one
          auto used_drv_info      = detected_drv_info;
          std::string used_driver = detected_driver;
          if (meter_in_config) {
            auto *sensor = this->wmbus_listeners_[meter_id];
            used_driver = ((sensor->type).empty() ? detected_driver : sensor->type);
            if (!(sensor->type).empty()){
              auto *used_drv_info_ptr = lookupDriver(used_driver);
              if (used_drv_info_ptr == nullptr) {
                used_driver = detected_driver;
                used_drv_info = detected_drv_info;
                ESP_LOGW(TAG, "Selected driver %s doesn't exist, using %s", (sensor->type).c_str(), used_driver.c_str());
              }
              else{
                used_drv_info = *used_drv_info_ptr;
                ESP_LOGI(TAG, "Using selected driver %s (detected driver was %s)", used_driver.c_str(), detected_driver.c_str());
              }
            }
          }

          this->led_blink();
          ESP_LOGI(TAG, "%s [0x%08x] RSSI: %ddBm T: %s %c1 %c",
                    (used_driver.empty()? "Unknown!" : used_driver.c_str()),
                    meter_id,
                    mbus_data.rssi,
                    telegram.c_str(),
                    mbus_data.mode,
                    mbus_data.block);

          if (meter_in_config) {
            bool supported_link_mode{false};
            if (used_drv_info.linkModes().empty()) {
              supported_link_mode = true;
              ESP_LOGW(TAG, "Link modes not defined in driver %s. Processing anyway.",
                      (used_driver.empty()? "Unknown!" : used_driver.c_str()));
            }
            else {
              supported_link_mode = ( ((mbus_data.mode == 'T') && (used_drv_info.linkModes().has(LinkMode::T1))) ||
                                      ((mbus_data.mode == 'C') && (used_drv_info.linkModes().has(LinkMode::C1))) );
            }

            if (used_driver.empty()) {
              ESP_LOGW(TAG, "Can't find driver for T: %s", telegram.c_str());
            }
            else if (!supported_link_mode) {
              ESP_LOGW(TAG, "Link mode %c1 not supported in driver %s",
                      mbus_data.mode,
                      used_driver.c_str());
            }
            else {
              auto *sensor = this->wmbus_listeners_[meter_id];
              
              bool id_match;
              MeterInfo mi;
              mi.parse("ESPHome", used_driver, t.addresses[0].id + ",", sensor->myKey);
              auto meter = createMeter(&mi);
              std::vector<Address> addresses;
              AboutTelegram about{"ESPHome wM-Bus", mbus_data.rssi, FrameType::WMBUS, this->frame_timestamp_};
              meter->handleTelegram(about, mbus_data.frame, false, &addresses, &id_match, &t);
              if (id_match) {
                for (auto const& field : sensor->fields) {
                  std::string field_name = field.first.first;
                  std::string unit = field.first.second;
                  if (field_name == "rssi") {
                    field.second->publish_state(mbus_data.rssi);
                  }
                  else if (field.second->get_unit_of_measurement().empty()) {
                    ESP_LOGW(TAG, "Fields without unit not supported as sensor, please switch to text_sensor.");
                  }
                  else {
                    Unit field_unit = toUnit(field.second->get_unit_of_measurement());
                    if (field_unit != Unit::Unknown) {
                      double value  = meter->getNumericValue(field_name, field_unit);
                      if (!std::isnan(value)) {
                        field.second->publish_state(value);
                      }
                      else {
                        ESP_LOGW(TAG, "Can't get requested field '%s' with unit '%s'", field_name.c_str(), unit.c_str());
                      }
                    }
                    else {
                      ESP_LOGW(TAG, "Can't get proper unit from '%s'", unit.c_str());
                    }
                  }
                }
                for (auto const& field : sensor->text_fields) {
                  if (meter->hasStringValue(field.first)) {
                    std::string value  = meter->getMyStringValue(field.first);
                    field.second->publish_state(value);
                  }
                  else {
                    ESP_LOGW(TAG, "Can't get requested field '%s'", field.first.c_str());
                  }
                }
#ifdef USE_WMBUS_MQTT
                std::string json;
                meter->printJsonMeter(&t, &json, false);
                std::string mqtt_topic = (App.get_friendly_name().empty() ? App.get_name() : App.get_friendly_name()) + "/wmbus/" + t.addresses[0].id;
                if (this->mqtt_client_.connect("", this->mqtt_->name.c_str(), this->mqtt_->password.c_str())) {
                  this->mqtt_client_.publish(mqtt_topic.c_str(), json.c_str(), this->mqtt_->retained);
                  ESP_LOGV(TAG, "Publish(topic='%s' payload='%s' retain=%d)", mqtt_topic.c_str(), json.c_str(), this->mqtt_->retained);
                  this->mqtt_client_.disconnect();
                }
                else {
                  ESP_LOGV(TAG, "Publish failed for topic='%s' (len=%u).", mqtt_topic.c_str(), json.length());
                }
#elif defined(USE_MQTT)
                std::string json;
                meter->printJsonMeter(&t, &json, false);
                std::string mqtt_topic = this->mqtt_client_->get_topic_prefix() + "/wmbus/" + t.addresses[0].id;
                this->mqtt_client_->publish(mqtt_topic, json);
#endif
              }
              else {
                ESP_LOGE(TAG, "Not for me T: %s", telegram.c_str());
              }
            }
          }
          else {
            // meter not in config
          }
        }
      }
# if defined(USE_WMBUS_MQTT) || defined(USE_MQTT)
      if (this->mqtt_raw) {
        send_mqtt_raw(t, mbus_data);
      }
#endif
    }
  }

  void WMBusComponent::register_wmbus_listener(const uint32_t meter_id, const std::string type, const std::string key) {
    if (this->wmbus_listeners_.count(meter_id) == 0) {
      WMBusListener *listener = new wmbus::WMBusListener(meter_id, type, key);
      this->wmbus_listeners_.insert({meter_id, listener});
    }
  }

  void WMBusComponent::led_blink() {
    if (this->led_pin_ != nullptr) {
      if (!this->led_on_) {
        this->led_on_millis_ = millis();
        this->led_pin_->digital_write(true);
        this->led_on_ = true;
      }
    }
  }

  void WMBusComponent::led_handler() {
    if (this->led_pin_ != nullptr) {
      if (this->led_on_) {
        if ((millis() - this->led_on_millis_) >= this->led_blink_time_) {
          this->led_pin_->digital_write(false);
          this->led_on_ = false;
        }
      }
    }
  }


#if defined(USE_WMBUS_MQTT) || defined(USE_MQTT)
  void WMBusComponent::send_mqtt_raw(Telegram &t, WMbusFrame &mbus_data) {
    bool is_parsed = !t.addresses.empty();
    if (!is_parsed && !this->mqtt_raw_parsed) {
      return;
    }

    std::string payload;
    std::string name = App.get_friendly_name().empty() ? App.get_name() : App.get_friendly_name();
    std::string mqtt_topic = str_sanitize(name) + "/wmbus/raw";

    if (this->mqtt_raw_prefix.size() > 0) {
      mqtt_topic = this->mqtt_raw_prefix + "/" + mqtt_topic;
    }

    if (is_parsed && this->mqtt_raw_parsed) {
      mqtt_topic += "/" + t.addresses[0].id;
    }

    switch(this->mqtt_raw_format) {
      case RAW_FORMAT_RTLWMBUS: 
        char telegram_time[24];
        strftime(telegram_time, sizeof(telegram_time), "%Y-%m-%d %H:%M:%S.00Z", gmtime(&(this->frame_timestamp_)));

        // Start building the payload
        payload += std::string(1, mbus_data.mode) + "1;1;1;" + telegram_time + ";" + std::to_string(mbus_data.rssi) + ";;;0x";

        // Add the formatted hex frame
        for (int i = 0; i < mbus_data.frame.size(); i++) {
          char hex_byte[3]; // 2 characters for hex + 1 for null terminator
          std::snprintf(hex_byte, sizeof(hex_byte), "%02X", mbus_data.frame[i]);
          payload += hex_byte;
        }

        break;
      
      default:
        payload += "{";
        if (is_parsed) {
          payload += "\"address\": \"" + t.addresses[0].id + "\", ";
        }
        payload += "\"mode\": \"" + std::string(1, mbus_data.mode) + "\", ";
        payload += "\"rssi\": " + std::to_string(mbus_data.rssi) + ", ";
        payload += "\"frame\": \"";
        for (int i = 0; i < mbus_data.frame.size(); i++) {
          char hex_byte[3]; // 2 characters for hex + 1 for null terminator
          std::snprintf(hex_byte, sizeof(hex_byte), "%02X", mbus_data.frame[i]);
          payload += hex_byte;
        }
        payload += "\"}";
  }

#ifdef USE_WMBUS_MQTT
    if (this->mqtt_client_.connect("", this->mqtt_->name.c_str(), this->mqtt_->password.c_str())) {
      this->mqtt_client_.publish(mqtt_topic.c_str(), payload.c_str(), this->mqtt_->retained);
      ESP_LOGV(TAG, "Publishing raw(topic='%s' payload='%s' retain=%d)", mqtt_topic.c_str(), payload.c_str(), this->mqtt_->retained);
      this->mqtt_client_.disconnect();
    }
    else {
      ESP_LOGV(TAG, "Publish failed raw for topic='%s' (len=%u).", mqtt_topic.c_str(), payload.length());
    }
#elif defined(USE_MQTT)
    this->mqtt_client_->publish(mqtt_topic, payload);
#endif
  }
#endif
  
  void WMBusComponent::send_to_clients(WMbusFrame &mbus_data) {
    for (auto & client : this->clients_) {
      switch (client.format) {
        case FORMAT_HEX:
          {
            switch (client.transport) {
              case TRANSPORT_TCP:
                {
                  ESP_LOGV(TAG, "Will send HEX telegram to %s:%d via TCP", client.ip.str().c_str(), client.port);
                  if (this->tcp_client_.connect(client.ip.str().c_str(), client.port)) {
                    this->tcp_client_.write((const uint8_t *) mbus_data.frame.data(), mbus_data.frame.size());
                    this->tcp_client_.stop();
                  }
                  else {
                    ESP_LOGE(TAG, "Can't connect via TCP to %s:%d", client.ip.str().c_str(), client.port);
                  }
                }
                break;
              case TRANSPORT_UDP:
                {
                  ESP_LOGV(TAG, "Will send HEX telegram to %s:%d via UDP", client.ip.str().c_str(), client.port);
                  this->udp_client_.beginPacket(client.ip.str().c_str(), client.port);
                  this->udp_client_.write((const uint8_t *) mbus_data.frame.data(), mbus_data.frame.size());
                  this->udp_client_.endPacket();
                }
                break;
              default:
                ESP_LOGE(TAG, "Unknown transport!");
                break;
            }
          }
          break;
        case FORMAT_RTLWMBUS:
          {
            char telegram_time[24];
            strftime(telegram_time, sizeof(telegram_time), "%Y-%m-%d %H:%M:%S.00Z", gmtime(&(this->frame_timestamp_)));
            switch (client.transport) {
              case TRANSPORT_TCP:
                {
                  ESP_LOGV(TAG, "Will send RTLWMBUS telegram to %s:%d via TCP", client.ip.str().c_str(), client.port);
                  if (this->tcp_client_.connect(client.ip.str().c_str(), client.port)) {
                    this->tcp_client_.printf("%c1;1;1;%s;%d;;;0x",
                                             mbus_data.mode,
                                             telegram_time,
                                             mbus_data.rssi);
                    for (int i = 0; i < mbus_data.frame.size(); i++) {
                      this->tcp_client_.printf("%02X", mbus_data.frame[i]);
                    }
                    this->tcp_client_.print("\n");
                    this->tcp_client_.stop();
                  }
                  else {
                    ESP_LOGE(TAG, "Can't connect via TCP to %s:%d", client.ip.str().c_str(), client.port);
                  }
                }
                break;
              case TRANSPORT_UDP:
                {
                  ESP_LOGV(TAG, "Will send RTLWMBUS telegram to %s:%d via UDP", client.ip.str().c_str(), client.port);
                  this->udp_client_.beginPacket(client.ip.str().c_str(), client.port);
                  this->udp_client_.printf("%c1;1;1;%s;%d;;;0x",
                                           mbus_data.mode,
                                           telegram_time,
                                           mbus_data.rssi);
                  for (int i = 0; i < mbus_data.frame.size(); i++) {
                    this->udp_client_.printf("%02X", mbus_data.frame[i]);
                  }
                  this->udp_client_.print("\n");
                  this->udp_client_.endPacket();
                }
                break;
              default:
                ESP_LOGE(TAG, "Unknown transport!");
                break;
            }
          }
          break;
        default:
          ESP_LOGE(TAG, "Unknown format!");
          break;
      }
    }
  }

  const LogString *WMBusComponent::format_to_string(Format format) {
    switch (format) {
      case FORMAT_HEX:
        return LOG_STR("hex");
      case FORMAT_RTLWMBUS:
        return LOG_STR("rtl-wmbus");
      default:
        return LOG_STR("unknown");
    }
  }

  const LogString *WMBusComponent::transport_to_string(Transport transport) {
    switch (transport) {
      case TRANSPORT_TCP:
        return LOG_STR("TCP");
      case TRANSPORT_UDP:
        return LOG_STR("UDP");
      default:
        return LOG_STR("unknown");
    }
  }

  void WMBusComponent::dump_config() {
    ESP_LOGCONFIG(TAG, "wM-Bus v%s-%s:", MY_VERSION, WMBUSMETERS_VERSION);
    if (this->clients_.size() > 0) {
      ESP_LOGCONFIG(TAG, "  Clients:");
      for (auto & client : this->clients_) {
        ESP_LOGCONFIG(TAG, "    %s: %s:%d %s [%s]",
                      client.name.c_str(),
                      client.ip.str().c_str(),
                      client.port,
                      LOG_STR_ARG(transport_to_string(client.transport)),
                      LOG_STR_ARG(format_to_string(client.format)));
      }
    }
    if (this->led_pin_ != nullptr) {
      ESP_LOGCONFIG(TAG, "  LED:");
      LOG_PIN("    Pin: ", this->led_pin_);
      ESP_LOGCONFIG(TAG, "    Duration: %d ms", this->led_blink_time_);
    }
#ifdef USE_ESP32
    ESP_LOGCONFIG(TAG, "  Chip ID: %012llX", ESP.getEfuseMac());
#endif
    ESP_LOGCONFIG(TAG, "  CC1101 frequency: %3.3f MHz", this->frequency_);
    ESP_LOGCONFIG(TAG, "  CC1101 SPI bus:");
    if (this->is_failed()) {
      ESP_LOGE(TAG, "   Check connection to CC1101!");
    }
    LOG_PIN("    MOSI Pin: ", this->spi_conf_.mosi);
    LOG_PIN("    MISO Pin: ", this->spi_conf_.miso);
    LOG_PIN("    CLK Pin:  ", this->spi_conf_.clk);
    LOG_PIN("    CS Pin:   ", this->spi_conf_.cs);
    LOG_PIN("    GDO0 Pin: ", this->spi_conf_.gdo0);
    LOG_PIN("    GDO2 Pin: ", this->spi_conf_.gdo2);
    std::string drivers = "";
    for (DriverInfo* p : allDrivers()) {
      drivers += p->name().str() + ", ";
    }
    drivers.erase(drivers.size() - 2);
    ESP_LOGCONFIG(TAG, "  Available drivers: %s", drivers.c_str());
    for (const auto &ele : this->wmbus_listeners_) {
      ele.second->dump_config();
    }
  }

  ///////////////////////////////////////

  void WMBusListener::dump_config() {
    std::string key = format_hex_pretty(this->key);
    key.erase(std::remove(key.begin(), key.end(), '.'), key.end());
    if (key.size()) {
      key.erase(key.size() - 5);
    }
    ESP_LOGCONFIG(TAG, "  Meter:");
    ESP_LOGCONFIG(TAG, "    ID: %zu [0x%08X]", this->id, this->id);
    ESP_LOGCONFIG(TAG, "    Type: %s", ((this->type).empty() ? "auto detect" : this->type.c_str()));
    ESP_LOGCONFIG(TAG, "    Key: '%s'", key.c_str());
    for (const auto &ele : this->fields) {
      ESP_LOGCONFIG(TAG, "    Field: '%s'", ele.first.first.c_str());
      LOG_SENSOR("     ", "Name:", ele.second);
    }
    for (const auto &ele : this->text_fields) {
      ESP_LOGCONFIG(TAG, "    Text field: '%s'", ele.first.c_str());
      LOG_TEXT_SENSOR("     ", "Name:", ele.second);
    }
  }

  WMBusListener::WMBusListener(const uint32_t id, const std::string type, const std::string key) {
    this->id = id;
    this->type = type;
    this->myKey = key;
    hex_to_bin(key, &(this->key));
  }

  int WMBusListener::char_to_int(char input)
  {
    if(input >= '0' && input <= '9') {
      return input - '0';
    }
    if(input >= 'A' && input <= 'F') {
      return input - 'A' + 10;
    }
    if(input >= 'a' && input <= 'f') {
      return input - 'a' + 10;
    }
    return -1;
  }

  bool WMBusListener::hex_to_bin(const char* src, std::vector<unsigned char> *target)
  {
    if (!src) return false;
    while(*src && src[1]) {
      if (*src == ' ' || *src == '#' || *src == '|' || *src == '_') {
        // Ignore space and hashes and pipes and underlines.
        src++;
      }
      else {
        int hi = char_to_int(*src);
        int lo = char_to_int(src[1]);
        if (hi<0 || lo<0) return false;
        target->push_back(hi*16 + lo);
        src += 2;
      }
    }
    return true;
  }

}  // namespace wmbus
}  // namespace esphome
