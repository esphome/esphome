#include "smartconfig_component.h"

#ifdef USE_ESP32
#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_smartconfig.h"
#endif

#ifdef USE_ESP8266
#include "smartconfig.h"
#endif

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#if defined(USE_ESP32) || defined(USE_ESP8266)

namespace esphome {
namespace smartconfig {

#ifdef USE_ESP32
#define SC_START_BIT BIT0
#define SC_READY_BIT BIT1
#define SC_DONE_BIT BIT2
#endif

static const char *const TAG = "smartconfig";
static wifi::WiFiAP connecting_sta_;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_ESP32
static EventGroupHandle_t sce_group = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static void smartconfig_event_handler(void *arg, esp_event_base_t event_base, int32_t event, void *event_data);
#endif

#ifdef USE_ESP8266
static bool is_sc_ready{false};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static void smartconfig_done(sc_status status, void *pdata);
#endif

static void smartconfig_got_ssid_pwd(void *data);

SmartConfigComponent::SmartConfigComponent() { global_smartconfig_component = this; }

float SmartConfigComponent::get_setup_priority() const { return setup_priority::WIFI; }

void SmartConfigComponent::config() {
  ESP_LOGD(TAG, "config SmartConfig");

#if defined(USE_ESP32)
  sce_group = xEventGroupCreate();
  esp_err_t err;
  esp_event_handler_instance_t instance_sc_id;
  err = esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID, &smartconfig_event_handler, nullptr,
                                            &instance_sc_id);
  ESP_ERROR_CHECK(err);
#elif defined(USE_ESP8266)
  smartconfig_set_type(SC_TYPE_ESPTOUCH);
  smartconfig_start(smartconfig_done);
  this->state_ = SmartConfigState::SC_START;
#endif
}

void SmartConfigComponent::loop() {
#if defined(USE_ESP32)
  if (sce_group == nullptr) {
    return;
  }
  EventBits_t xbits;
  xbits = xEventGroupWaitBits(sce_group, SC_START_BIT | SC_READY_BIT | SC_DONE_BIT, true, false, 0);
  if (xbits & SC_START_BIT) {
    ESP_LOGD(TAG, "Starting SmartConfig");
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
    esp_smartconfig_start(&cfg);
    this->state_ = SmartConfigState::SC_START;
  }
  if (xbits & SC_READY_BIT) {
    ESP_LOGD(TAG, "Smartconfig ready");
    this->state_ = SmartConfigState::SC_READY;
    on_smartconfig_ready_.call();
  }
  if (xbits & SC_DONE_BIT) {
    ESP_LOGD(TAG, "Smartconfig over");
    esp_smartconfig_stop();
    this->state_ = SmartConfigState::SC_DONE;
    this->set_timeout("save-wifi", 1000, [] {
      wifi::global_wifi_component->save_wifi_sta(connecting_sta_.get_ssid(), connecting_sta_.get_password());
    });
  }
#elif defined(USE_ESP8266)
  if (is_sc_ready && (this->state_ != SmartConfigState::SC_READY)) {
    this->state_ = SmartConfigState::SC_READY;
    on_smartconfig_ready_.call();
  } else if (!is_sc_ready && (this->state_ == SmartConfigState::SC_READY) &&
             wifi::global_wifi_component->is_connected()) {
    this->state_ = SmartConfigState::SC_DONE;
    this->set_timeout("save-wifi", 1000, [] {
      wifi::global_wifi_component->save_wifi_sta(connecting_sta_.get_ssid(), connecting_sta_.get_password());
    });
  }
#endif
}

#ifdef USE_ESP32
void SmartConfigComponent::start() {
  if (this->state_ == SmartConfigState::SC_IDLE) {
    xEventGroupSetBits(sce_group, SC_START_BIT);
  }
}

void smartconfig_event_handler(void *arg, esp_event_base_t event_base, int32_t event, void *event_data) {
  if (event_base == SC_EVENT && event == SC_EVENT_SCAN_DONE) {
    ESP_LOGD(TAG, "SC_EVENT_SCAN_DONE");
    xEventGroupSetBits(sce_group, SC_READY_BIT);
  } else if (event_base == SC_EVENT && event == SC_EVENT_FOUND_CHANNEL) {
    ESP_LOGD(TAG, "Found channel");
  } else if (event_base == SC_EVENT && event == SC_EVENT_GOT_SSID_PSWD) {
    smartconfig_got_ssid_pwd(event_data);
  } else if (event_base == SC_EVENT && event == SC_EVENT_SEND_ACK_DONE) {
    xEventGroupSetBits(sce_group, SC_DONE_BIT);
  }
}
#endif

static void smartconfig_got_ssid_pwd(void *data) {
#if defined(USE_ESP32)
  smartconfig_event_got_ssid_pswd_t *sta_conf = (smartconfig_event_got_ssid_pswd_t *) data;
#elif defined(USE_ESP8266)
  struct station_config *sta_conf = (struct station_config *) data;
#endif
  std::string ssid((char *) sta_conf->ssid);
  std::string password((char *) sta_conf->password);
  connecting_sta_.set_ssid(ssid);
  connecting_sta_.set_password(password);

  wifi::global_wifi_component->set_sta(connecting_sta_);
  wifi::global_wifi_component->start_scanning();
  ESP_LOGD(TAG, "Received smartconfig wifi settings ssid=%s, password=" LOG_SECRET("%s"), ssid.c_str(),
           password.c_str());
}

#ifdef USE_ESP8266
void smartconfig_done(sc_status status, void *pdata) {
  switch (status) {
    case SC_STATUS_WAIT:
      ESP_LOGD(TAG, "SC_STATUS_WAIT");
      break;
    case SC_STATUS_FIND_CHANNEL:
      ESP_LOGD(TAG, "SC_STATUS_FIND_CHANNEL");
      is_sc_ready = true;
      break;
    case SC_STATUS_GETTING_SSID_PSWD:
      ESP_LOGD(TAG, "SC_STATUS_GETTING_SSID_PSWD");
      break;
    case SC_STATUS_LINK:
      ESP_LOGD(TAG, "SC_STATUS_LINK");
      smartconfig_got_ssid_pwd(pdata);
      break;
    case SC_STATUS_LINK_OVER:
      ESP_LOGD(TAG, "SC_STATUS_LINK_OVER");
      smartconfig_stop();
      is_sc_ready = false;
      break;
  }
}
#endif

SmartConfigComponent *global_smartconfig_component =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    nullptr;

SmartConfigReadyTrigger::SmartConfigReadyTrigger(SmartConfigComponent *&sc_component) {
  sc_component->add_on_ready([this]() {
    ESP_LOGD(TAG, "Smartconfig is ready");
    this->trigger();
  });
}

}  // namespace smartconfig
}  // namespace esphome

#endif
