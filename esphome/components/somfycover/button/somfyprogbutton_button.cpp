#include "somfyprogbutton_button.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <SomfyRemote.h>

#define CC1101_FREQUENCY 433.42
#define EMITTER_GPIO 4

namespace esphome {
namespace somfyprogbutton {
  

SomfyRemote *somfyremote_;
static const char *const TAG = "SomfyButton.button";


void SomfyProgButton::dump_config() {
  LOG_BUTTON("", "ArduinoSomfy Button", this);
  ESP_LOGCONFIG(TAG, "  RemoteID: 0x%06x", this->remote_id_);
}

void SomfyProgButton::setup() {
  pinMode(EMITTER_GPIO, OUTPUT);
  digitalWrite(EMITTER_GPIO, LOW);
  //ELECHOUSE_cc1101.setSpiPin(14, 12, 13, 15);
  ELECHOUSE_cc1101.setSpiPin(14, 12, 2, 15);
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setMHZ(CC1101_FREQUENCY);
  SomfyRemote* somfyremote_ = new SomfyRemote(EMITTER_GPIO, this->remote_id_);
  ELECHOUSE_cc1101.SetTx();
}
float SomfyProgButton::get_setup_priority() const { return setup_priority::DATA; }
void SomfyProgButton::press_action(){
  Command command = Command::Prog;
  Serial.print("Rolling code: ");
  uint16_t newcode = id(this->rollingCodeStorage_);
  // Serial.printf("%u\n", (unsigned int)newcode);
  somfyremote_->sendCommand(command, this->remote_id_, newcode);
  id(this->rollingCodeStorage_)+=1;
}

}  // namespace somfyprogbutton
}  // namespace esphome
