#include "wiegand_reader.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wiegand_reader {

static const char *TAG = "wiegand_reader";

void ICACHE_RAM_ATTR WiegandReader::pin_state_changed_(WiegandReader *reader) {
  reader->wiegand_.setPin0State(reader->pin_d0_->digital_read());
  reader->wiegand_.setPin1State(reader->pin_d1_->digital_read());
}

void WiegandReader::received_data_(uint8_t* data, uint8_t bits, WiegandReader *reader) {
  uint8_t byte_count = (bits + 7) / 8;

  String code = "";
  for (int i=0; i<byte_count; i++) {
    code = code + String(data[i], 16);
  }
  ESP_LOGD(TAG, "Data received : %s", code.c_str());

  for (auto *trigger : reader->triggers_)
    trigger->process(code);
}

void WiegandReader::received_data_error_(Wiegand::DataError error, uint8_t* raw_data, uint8_t raw_bits, const char* message) {
  ESP_LOGE(TAG, "FAILED : %s", message);
  ESP_LOGE(TAG, "   ERROR : %s", Wiegand::DataErrorStr(error));

  uint8_t byte_count = (raw_bits + 7) / 8;
  ESP_LOGE(TAG, "   BYTE COUNT : %i", byte_count);

  String code = "";
  for (int i=0; i<byte_count; i++) {
    code = code + String(raw_data[i], 16);
  }
  ESP_LOGE(TAG, "   DECODED : %s", code.c_str());
}

void WiegandReader::setup() {
  this->pin_d0_->pin_mode(INPUT);
  this->pin_d1_->pin_mode(INPUT);
  this->wiegand_.onReceive(WiegandReader::received_data_, this);
  this->wiegand_.onReceiveError(WiegandReader::received_data_error_, "Card read error: ");

  this->dump_config();

  this->wiegand_.begin(0xFF, true);

  this->pin_d0_->attach_interrupt(WiegandReader::pin_state_changed_, this, CHANGE);
  this->pin_d1_->attach_interrupt(WiegandReader::pin_state_changed_, this, CHANGE);
  this->pin_state_changed_(this);
}

void WiegandReader::update() {
  InterruptLock lock;
  this->wiegand_.flush();
}

void WiegandReader::set_data_pins(GPIOPin *pin_d0, GPIOPin *pin_d1){
    this->pin_d0_ = pin_d0;
    this->pin_d1_ = pin_d1;
}

float WiegandReader::get_setup_priority() const { return setup_priority::DATA; }

void WiegandReader::dump_config() {
  ESP_LOGCONFIG(TAG, "WiegandReader:");
  LOG_PIN("  DO Pin: ", this->pin_d0_);
  LOG_PIN("  D1 Pin: ", this->pin_d1_);
  LOG_UPDATE_INTERVAL(this);
}

void WiegandReaderTrigger::process(String tag) {
  this->trigger(tag.c_str());
}

}  // namespace wiegand_reader
}  // namespace esphome
