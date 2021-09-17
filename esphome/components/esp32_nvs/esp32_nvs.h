
#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#ifdef ARDUINO_ARCH_ESP32
#include "nvsflasher.h"

// override the log level for debugging
// #undef LOG_LOCAL_LEVEL
// #define LOG_LOCAL_LEVEL ESP_LOG_INFO

using std::string;

//debug to dump nvs to serial port
extern "C" void nvs_dump(const char *partName);

namespace esphome {
namespace esp32_nvs {

static const char *TAG = "esp32_nvs";

template<typename T> class Esp32NvsComponent : public Component {
public:
  using value_type = T;

  explicit Esp32NvsComponent(T initial_value) : initial_value_(initial_value) {  }

  T &value() { return this->value_; }
  string &get_id() { return this->id_; }
  string &get_namespace() { return this->nvs_namespace_; }

  void setup() override {  }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void loop() override {  }

  void dump_config() {
    string myval = to_string(this->value_);
    ESP_LOGCONFIG(TAG, "%s/%s = %s", this->nvs_namespace_.c_str(), this->id_.c_str(), myval.c_str());
  }

  // needed to convert T into a character buffer suitable for nvs functions
  // (<string> is handled in a specialized template, below)
  void prepare_buffers(bool update, string ns, string key, T new_value) {
    uint32_t len = sizeof(T);
    // ESP_LOGI(TAG, "prepare int: len=%d", len);
    unsigned char* rd_bfr = new unsigned char[len];
    unsigned char* wr_bfr = new unsigned char[len];
    memcpy(wr_bfr, &new_value, len);
    NvsResultBfrType success = NONE;
    success = NvsFlasher::reconcile_flash(ns.c_str(), key.c_str(), rd_bfr, len, wr_bfr, len, update);
    if(success == READ_BFR)
      memcpy(&this->value_, rd_bfr, len);
    else if(success == WRITE_BFR)
      memcpy(&this->value_, wr_bfr, len);
    delete[] rd_bfr;
    delete[] wr_bfr;
  }

  // save the namespace and key name at initialization for this id
  void set_nvs_ns_key(string nvs_namespace, string key) {
    this->nvs_namespace_ = nvs_namespace;
    this->id_ = key;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    //ESP_LOGI(TAG, "set_nvs_ns_key = %s/%s", nvs_namespace_.c_str(), id_.c_str());

    // initial_value_ was saved in the constructor
    this->prepare_buffers(false, nvs_namespace, key, initial_value_);

    //uncomment nvs_dump to dump the entire nvs partition to the debug port
    //nvs_dump("nvs");
  }

protected:
  T value_{};
  T initial_value_;
  string nvs_namespace_;
  string id_;
};

// specialized template for handling 'string' since converting it between
// char[] is very different from the other types
template <>
inline void Esp32NvsComponent<string>::prepare_buffers(bool update, string ns, string key, string new_value) {
  uint32_t wr_len = new_value.length() + 1; //+1 for 0x0 terminator
  uint32_t rd_len = NvsFlasher::get_required_size(ns.c_str(), key.c_str());
  if(rd_len == 0)
    rd_len = wr_len;
  unsigned char* rd_bfr = new unsigned char[rd_len];
  unsigned char* wr_bfr = new unsigned char[wr_len];
  memcpy(wr_bfr, new_value.c_str(), wr_len);
  // ESP_LOGI(TAG, "prepare string: rd_len=%d, wr_len=%d", rd_len, wr_len);
  NvsResultBfrType success = NONE;
  success = NvsFlasher::reconcile_flash(ns.c_str(), key.c_str(), rd_bfr, rd_len, wr_bfr, wr_len, update);
  if(success == READ_BFR)
      this->value_ = (char *)rd_bfr; // this does a copy. rd_bfr can now be deleted
  else if(success == WRITE_BFR)
      this->value_ = (char *)wr_bfr; // this does a copy. wr_bfr can now be deleted
  delete[] rd_bfr;
  delete[] wr_bfr;
  // ESP_LOGI(TAG, "%s/%s value = %s", ns.c_str(), key.c_str(), this->value_.c_str());
}

// esp32nvs.set
// set the id named in the yaml
template<class C, typename... Ts> class Esp32NvsVarSetAction : public Action<Ts...> {
public:
  explicit Esp32NvsVarSetAction(C *parent) : parent_(parent) {}

  using T = typename C::value_type;

  TEMPLATABLE_VALUE(T, value);

  void play(Ts... x) override 
  { 
    this->parent_->prepare_buffers(true, 
                                  this->parent_->get_namespace(), 
                                  this->parent_->get_id(), 
                                  this->value_.value(x...));
    this->parent_->dump_config();
  }

protected:
  C *parent_;
};

// esp32nvs.erase
template<class C, typename... Ts> class Esp32NvsEraseAction : public Action<Ts...> {
public:
  explicit Esp32NvsEraseAction(C *parent) : parent_(parent) {}

  using T = typename C::value_type;

  TEMPLATABLE_STRING_VALUE(value);

  void play(Ts... x) override 
  { 
    NvsFlasher::erase_namespace(this->parent_->get_namespace().c_str());
  }

protected:
  C *parent_;
};

// esp32nvs.set_id
// set the id passed as a parameter of the Action
template<class C, typename... Ts> class Esp32NvsSetIdAction : public Action<Ts...> {
public:
  explicit Esp32NvsSetIdAction(C *parent) : parent_(parent) {}

  using T = typename C::value_type;

  TEMPLATABLE_VALUE(T, value);

  void play(Ts... x) override 
  { 
    this->parent_->prepare_buffers(true, x...);
    this->parent_->dump_config();
  }

protected:
  C *parent_;
};

template<typename T>
T &id(Esp32NvsComponent<T> *value) { return value->value(); }

}  // namespace esp32_nvs
}  // namespace esphome

#endif //ARDUINO_ARCH_ESP32
