#include "storage.h"
#include "esphome/core/log.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace storage {

static const char *const TAG = "storage";

void MediaDetectInterrupt::inserted(MediaDetectInterrupt *data) { data->present = true; }
void MediaDetectInterrupt::ejected(MediaDetectInterrupt *data) { data->present = false; }

/** --------------------------------------------------------------------------------
 *
 * @brief   Card eject/insert interrupt data
 *
 */
// struct MediaDetectInterrupt {
//   volatile bool present{true};
//   bool init{false};
//   static void inserted(MediaDetectInterrupt *data);
//   static void ejected(MediaDetectInterrupt *data);
// };

/** --------------------------------------------------------------------------------
 *
 * @brief  Attach inttterupt handler to the cd_pin if defined
 *
 */
bool Storage::init_media_state_interrupt() {
  if (this->cd_pin_ != NULL) {
    this->cd_pin_->setup();
    this->cd_pin_->pin_mode(gpio::FLAG_PULLUP);
    this->cd_pin_->attach_interrupt(MediaDetectInterrupt::inserted, &this->media_present_st_,
                                    gpio::INTERRUPT_LOW_LEVEL);
    this->cd_pin_->attach_interrupt(MediaDetectInterrupt::ejected, &this->media_present_st_,
                                    gpio::INTERRUPT_HIGH_LEVEL);

    // Set current pin state
    this->media_present_st_.present = this->cd_pin_->digital_read();
    this->media_present_st_.init = true;
    ESP_LOGD(TAG, "Arm media detect interrupt");
    return true;
  }
  return false;
}

/** --------------------------------------------------------------------------------
 *
 * @brief   Rerturn media state received by interrupt
 *
 */
StorageIntState Storage::media_interrupt_state() {
  //  If interrupt armed
  if (this->media_present_st_.init) {
    if (this->media_present_st_.present != cur_mstate_) {
      cur_mstate_ = this->media_present_st_.present;
    }

    if (cur_mstate_) {
      return StorageIntState::MEDIA_PRESENT;  // cur_mstate_ == true
    } else {
      return StorageIntState::MEDIA_ABSENT;  // cur_mstate_ == false
    }
  }
  //  If interrupt NOT armed
  return StorageIntState::MEDIA_UNUSED;
}

/***********************************************************************************
 *
 *   Action and condition processint
 */
template<typename... Ts> class StorageIsPresent : public Condition<Ts...> {
 public:
  StorageIsPresent(Storage *parent) : parent_(parent) {}
  bool check(Ts... x) override { return this->parent_->state_media(); }

 protected:
  Storage *parent_;
};

}  // namespace storage
}  // namespace esphome
